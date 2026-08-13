#pragma once

#include "connection.h"
#include "db_manager.h"
#include "session_registry.h"

#include "core/pointer_types.h"
#include "network/cargo_socket.h"

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
  ConnectionPool(SessionRegistry &sessions, DBManager &database, QObject *parent = nullptr);
  ~ConnectionPool();

  int count() const;

  void create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, const QString &ipid);

  void clear();

Q_SIGNALS:
  void connectionAttempted(const QString &address, const QString &ipid, const QString &hdid);

private:
  SessionRegistry &_sessions;
  DBManager &_database;
  QList<Connection *> _connections;
};
} // namespace kenji
