#include "connection.h"

#include "config_manager.h"
#include "db_manager.h"
#include "protocol/protocol_info.h"

kenji::Connection::Connection(SessionRegistry &sessions, DBManager &database, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, const QString &ipid, QObject *parent)
    : QObject{parent}
    , _sessions{sessions}
    , _database{database}
    , _socket{socket}
    , _address{address}
    , _ipid{ipid}
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

  _hdid = packet.hdid;
  Q_EMIT connectionAttempted(_address.toString(), _ipid, _hdid);
  auto ban = _database.isHDIDBanned(_hdid);
  if (ban.first)
  {
    drop(theory::ErrorPacket::Banned, "Reason: " + ban.second.reason + "\nBan ID: " + QString::number(ban.second.id) + "\nUntil: " + ban.second.until());
    return;
  }

  _router.unregisterAllRoutes();
  _router.registerRoute<theory::SessionClaimPacket>(&Connection::process, this);
}

void kenji::Connection::process(const theory::SessionClaimPacket &packet)
{
  const auto ticket = _sessions.join(packet.sessionToken, _hdid, _socket, _address);
  if (!ticket)
  {
    drop(theory::ErrorPacket::ServerFull);
    return;
  }
  finishHandshake(ticket.value());
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
  client->m_ipid = _ipid;

  theory::SessionGrantPacket grant;
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
