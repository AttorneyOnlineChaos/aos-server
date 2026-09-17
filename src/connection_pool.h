#pragma once

#include "badge/badge_gateway.h"
#include "connection.h"
#include "core/pointer_types.h"
#include "network/cargo_socket.h"
#include "server_database.h"
#include "session_registry.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>

namespace kenji
{
class ConnectionPool : public QObject
{
  Q_OBJECT

public:
  ConnectionPool(theory::BadgeGateway &gateway, SessionRegistry &sessions, ServerDatabase &database, QObject *parent = nullptr);
  ~ConnectionPool();

  int count() const;

  void create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address);

  void clear();

private:
  theory::BadgeGateway &_gateway;
  SessionRegistry &_sessions;
  ServerDatabase &_database;
  QList<Connection *> _connections;
};
} // namespace kenji
