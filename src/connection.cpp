#include "connection.h"

#include "config_manager.h"
#include "protocol/protocol_info.h"
#include "server_database.h"

#include <QUuid>

kenji::Connection::Connection(theory::BadgeGateway &gateway, SessionRegistry &sessions, ServerDatabase &database, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, QObject *parent)
    : QObject{parent}
    , _gateway{gateway}
    , _sessions{sessions}
    , _database{database}
    , _socket{socket}
    , _address{address}
{
  _router.registerRoute<theory::HelloPacket>(&Connection::process, this);

  _deadline.setSingleShot(true);
  connect(&_deadline, &QTimer::timeout, this, [this] { drop(theory::ErrorPacket::ProtocolError, "The handshake has timed out."); });

  connect(_socket.get(), &theory::CargoSocket::pendingPacketAvailable, this, &Connection::processPendingPackets);
  connect(_socket.get(), &theory::CargoSocket::disconnectedFromPeer, this, &Connection::finish);
  connect(_socket.get(), &theory::CargoSocket::errorOccurred, this, [this](const theory::CargoError &error) {
    switch (error.code)
    {
    default:
    case theory::CargoError::SocketError:
    case theory::CargoError::SslError:
      finish();
      break;
    case theory::CargoError::MalformedMessage:
    case theory::CargoError::InvalidStructure:
    case theory::CargoError::InvalidPacket:
      drop(theory::ErrorPacket::ProtocolError, error.toString());
      break;
    }
  });
}

void kenji::Connection::beginHandshake()
{
  if (!_socket->isConnected())
  {
    finish();
    return;
  }

  _deadline.start(ConfigManager::handshakeTimeout() * 1000);
  processPendingPackets();
}

void kenji::Connection::finish()
{
  if (_finished)
  {
    return;
  }

  _finished = true;
  _deadline.stop();
  if (_gatekeeper)
  {
    _gatekeeper->cancel();
  }

  if (_socket)
  {
    _socket->disconnect(this);
    _socket->close();
  }

  Q_EMIT finished();
}

void kenji::Connection::drop(theory::ErrorPacket::Code code, const QString &reason)
{
  if (_finished)
  {
    return;
  }

  theory::ErrorPacket error;
  error.code = code;
  error.what = reason;
  _socket->shipPacket(error);
  finish();
}

void kenji::Connection::process(const theory::HelloPacket &packet)
{
  if (packet.protocolVersion != theory::protocolVersion())
  {
    drop(theory::ErrorPacket::ProtocolError, "Incompatible protocol version.");
    return;
  }

  _router.unregisterAllRoutes();
  _router.registerRoute<theory::SessionClaimPacket>(&Connection::process, this);
}

void kenji::Connection::process(const theory::SessionClaimPacket &packet)
{
  _sessionToken = packet.sessionToken;
  _router.unregisterAllRoutes();
  _router.registerRoute<theory::BadgeSelectPacket>(&Connection::process, this);
  _router.registerRoute<theory::BadgePacket>(&Connection::process, this);
  _gatekeeper = _gateway.create(_address, QUuid::createUuid());
  connect(_gatekeeper.get(), &theory::BadgeGatekeeper::selectionReady, this, &Connection::shipBadgeSelection);
  connect(_gatekeeper.get(), &theory::BadgeGatekeeper::challengeReady, this, &Connection::shipBadgeChallenge);
  connect(_gatekeeper.get(), &theory::BadgeGatekeeper::admitted, this, &Connection::admitPlayer);
  connect(_gatekeeper.get(), &theory::BadgeGatekeeper::refused, this, &Connection::refusePlayer);
  _deadline.stop();
  _gatekeeper->start(packet.userToken);
}

void kenji::Connection::process(const theory::BadgeSelectPacket &packet)
{
  _gatekeeper->processSelect(packet.badgeId);
}

void kenji::Connection::process(const theory::BadgePacket &packet)
{
  _gatekeeper->processResponse(packet.badgeId, packet.payload);
}

void kenji::Connection::shipBadgeSelection(const QStringList &badgeIds)
{
  theory::BadgeSelectionPacket selection;
  selection.badgeIds = badgeIds;
  _socket->shipPacket(selection);
}

void kenji::Connection::shipBadgeChallenge(const QString &badgeId, const QJsonObject &challengeData)
{
  theory::BadgePacket badge;
  badge.badgeId = badgeId;
  badge.payload = challengeData;
  _socket->shipPacket(badge);
}

void kenji::Connection::admitPlayer(const theory::UserDatabase::Ticket &ticket)
{
  if (ticket.user.id != theory::NoUserId)
  {
    const std::optional<QList<theory::BanRecord>> bans = _database.activeBans(ticket.user.id);
    if (!bans)
    {
      drop(theory::ErrorPacket::ServerFull, "Ban status could not be verified. Please try again shortly.");
      return;
    }

    if (!bans->isEmpty())
    {
      const theory::BanRecord &ban = bans->first();
      drop(theory::ErrorPacket::Banned, "Reason: " + ban.reason + "\nBan ID: " + QString::number(ban.id) + "\nUntil: " + ban.until());
      return;
    }
  }

  _userToken = ticket.token;
  const auto session = _sessions.join(ticket, _sessionToken, _socket, _address);
  if (!session)
  {
    drop(theory::ErrorPacket::ServerFull);
    return;
  }

  finishHandshake(session.value());
}

void kenji::Connection::refusePlayer(const theory::BadgeError &error)
{
  switch (error.code)
  {
  default:
  case theory::BadgeError::Internal:
    drop(theory::ErrorPacket::ProtocolError, QStringLiteral("internal error"));
    break;
  case theory::BadgeError::Denied:
    drop(theory::ErrorPacket::Unauthorized, error.what);
    break;
  case theory::BadgeError::OutOfRange:
    drop(theory::ErrorPacket::ProtocolError, error.what);
    break;
  }
}

void kenji::Connection::finishHandshake(const SessionRegistry::Ticket &ticket)
{
  AOClient *client = ticket.client;

  auto previous = client->socket();
  client->setSocket(_socket);
  client->markActive();
  if (previous != _socket && previous->isConnected())
  {
    theory::ErrorPacket packet;
    packet.code = theory::ErrorPacket::SessionTransfered;
    previous->shipPacket(packet);
    previous->close();
  }

  client->m_remote_ip = _address;

  theory::SessionGrantPacket grant;
  grant.userToken = _userToken;
  grant.sessionToken = ticket.token;
  grant.result = ticket.recovered ? theory::SessionGrantPacket::Recovered : theory::SessionGrantPacket::Fresh;
  client->shipPacket(grant);

  if (ticket.recovered)
  {
    client->resumeSession();
  }
  else
  {
    client->beginSession();
  }

  _deadline.stop();
  _client = client;
}

void kenji::Connection::processPendingPackets()
{
  while (!_finished && _socket && _socket->hasPendingPacket())
  {
    const theory::PacketPointer packet = _socket->nextPacket();
    if (auto error = packet->verify())
    {
      drop(theory::ErrorPacket::ProtocolError, error->toString());
      return;
    }

    if (_client)
    {
      if (!_client->processPendingPacket(*packet))
      {
        return;
      }
    }
    else if (!_router.route(*packet))
    {
      drop(theory::ErrorPacket::ProtocolError, "Invalid packet.");
      return;
    }
  }
}
