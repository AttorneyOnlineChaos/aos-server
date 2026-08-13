#include "connection_pool.h"

#include "connection.h"

kenji::ConnectionPool::ConnectionPool(SessionRegistry &sessions, DBManager &database, QObject *parent)
    : QObject{parent}
    , _sessions{sessions}
    , _database{database}
{}

kenji::ConnectionPool::~ConnectionPool()
{
  clear();
}

int kenji::ConnectionPool::count() const
{
  return _connections.size();
}

void kenji::ConnectionPool::clear()
{
  auto connections = std::move(_connections);
  for (Connection *connection : connections)
  {
    connection->disconnect(this);
    connection->finish();
    connection->deleteLater();
  }
}

void kenji::ConnectionPool::create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address, const QString &ipid)
{
  Connection *connection = new Connection(_sessions, _database, socket, address, ipid, this);
  _connections.append(connection);

  connect(connection, &Connection::connectionAttempted, this, &ConnectionPool::connectionAttempted);
  connect(connection, &Connection::finished, this, [this, connection] {
    _connections.removeOne(connection);
    connection->deleteLater();
  });

  connection->beginHandshake();
}
