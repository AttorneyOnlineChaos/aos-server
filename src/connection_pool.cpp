#include "connection_pool.h"

#include "connection.h"

kenji::ConnectionPool::ConnectionPool(theory::BadgeGateway &gateway, SessionRegistry &sessions, ServerDatabase &database, QObject *parent)
    : QObject{parent}
    , _gateway{gateway}
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

void kenji::ConnectionPool::create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address)
{
  Connection *connection = new Connection(_gateway, _sessions, _database, socket, address, this);
  _connections.append(connection);

  connect(connection, &Connection::finished, this, [this, connection] {
    _connections.removeOne(connection);
    connection->deleteLater();
  });

  connection->beginHandshake();
}
