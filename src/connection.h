#pragma once

#include "badge/badge_error.h"
#include "badge/badge_gatekeeper.h"
#include "badge/badge_gateway.h"
#include "badge/user_database.h"
#include "core/pointer_types.h"
#include "network/cargo_socket.h"
#include "network/packet_router.h"
#include "protocol/packets/handshake_packets.h"
#include "protocol/packets/moderation_packets.h"
#include "protocol/packets/session_packets.h"
#include "server_database.h"
#include "session_registry.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <optional>

namespace kenji
{
class Connection : public QObject
{
  Q_OBJECT

public:
  Connection(theory::BadgeGateway &gateway, SessionRegistry &sessions, ServerDatabase &database, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, QObject *parent = nullptr);

  void beginHandshake();
  void finish();

Q_SIGNALS:
  void finished();

private:
  theory::BadgeGateway &_gateway;
  SessionRegistry &_sessions;
  ServerDatabase &_database;
  theory::Shared<theory::CargoSocket> _socket;
  QHostAddress _address;

  theory::PacketRouter _router;
  QTimer _deadline;
  bool _finished = false;
  AOClient *_client = nullptr;

  theory::Unique<theory::BadgeGatekeeper> _gatekeeper;
  std::optional<QString> _sessionToken;
  QString _userToken;

  void drop(theory::ErrorPacket::Code code, const QString &reason = QString());

  void finishHandshake(const SessionRegistry::Ticket &ticket);

  void process(const theory::HelloPacket &packet);
  void process(const theory::SessionClaimPacket &packet);
  void process(const theory::BadgeSelectPacket &packet);
  void process(const theory::BadgePacket &packet);
  void shipBadgeSelection(const QStringList &badgeIds);
  void shipBadgeChallenge(const QString &badgeId, const QJsonObject &challengeData);
  void admitPlayer(const theory::UserDatabase::Ticket &ticket);
  void refusePlayer(const theory::BadgeError &error);

private Q_SLOTS:
  void processPendingPackets();
};
} // namespace kenji
