#pragma once

#include "core/pointer_types.h"
#include "db_manager.h"
#include "network/cargo_socket.h"
#include "network/packet_router.h"
#include "protocol/packets/handshake_packets.h"
#include "protocol/packets/moderation_packets.h"
#include "protocol/packets/session_packets.h"
#include "session_registry.h"

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>

namespace kenji
{
class Connection : public QObject
{
  Q_OBJECT

public:
  Connection(SessionRegistry &sessions, DBManager &database, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, const QString &ipid, QObject *parent = nullptr);

  void beginHandshake();
  void finish();

Q_SIGNALS:
  void connectionAttempted(const QString &address, const QString &ipid, const QString &hdid);
  void finished();

private:
  SessionRegistry &_sessions;
  DBManager &_database;
  theory::Shared<theory::CargoSocket> _socket;
  QHostAddress _address;
  QString _ipid;
  QString _hdid;

  theory::PacketRouter _router;
  QTimer _deadline;
  bool _finished = false;
  AOClient *_client = nullptr;

  void drop(theory::ErrorPacket::Code code, const QString &reason = QString());

  void finishHandshake(const SessionRegistry::Ticket &ticket);

  void process(const theory::HelloPacket &packet);
  void process(const theory::SessionClaimPacket &packet);

private Q_SLOTS:
  void processPendingPackets();
};
} // namespace kenji
