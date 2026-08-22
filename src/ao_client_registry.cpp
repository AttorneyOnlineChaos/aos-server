#include "ao_client_registry.h"

kenji::AOClientRegistry::AOClientRegistry(Server &server, ULogger &logger, MusicManager &musicManager, int capacity, QObject *parent)
    : QObject{parent}
    , _server{server}
    , _logger{logger}
    , _musicManager{musicManager}
    , _capacity{capacity}
{
  for (int i = _capacity - 1; i >= 0; i--)
  {
    _availableIds.push(i);
  }
}

int kenji::AOClientRegistry::capacity() const
{
  return _capacity;
}

int kenji::AOClientRegistry::count() const
{
  return _clients.size();
}

QList<kenji::AOClient *> kenji::AOClientRegistry::clients() const
{
  return _clients.values();
}

kenji::AOClient *kenji::AOClientRegistry::client(int id) const
{
  return _clients.value(id);
}

QList<kenji::AOClient *> kenji::AOClientRegistry::clientsIf(const Condition &condition) const
{
  QList<AOClient *> matches;
  for (AOClient *client : _clients)
  {
    if (condition(client))
    {
      matches.append(client);
    }
  }
  return matches;
}

int kenji::AOClientRegistry::countIf(const Condition &condition) const
{
  return clientsIf(condition).length();
}

QList<kenji::AOClient *> kenji::AOClientRegistry::clientsByIpid(const QString &ipid) const
{
  return clientsIf([&ipid](const AOClient *client) { return client->getIpid() == ipid; });
}

QList<kenji::AOClient *> kenji::AOClientRegistry::clientsByHwid(const QString &hwid) const
{
  return clientsIf([&hwid](const AOClient *client) { return client->getHwid() == hwid; });
}

int kenji::AOClientRegistry::countByAddress(const QHostAddress &address) const
{
  return countIf([&address](const AOClient *client) { return address.isEqual(client->m_remote_ip); });
}

kenji::AOClient *kenji::AOClientRegistry::create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address)
{
  if (_availableIds.isEmpty())
  {
    return nullptr;
  }

  const int id = _availableIds.pop();
  AOClient *client = new AOClient(&_server, _logger, socket, address, this, id, &_musicManager);
  _clients.insert(id, client);
  Q_EMIT clientAdded(client);
  return client;
}

void kenji::AOClientRegistry::remove(AOClient *client)
{
  if (!client || _clients.value(client->clientId()) != client)
  {
    return;
  }

  _clients.remove(client->clientId());
  _availableIds.push(client->clientId());
  Q_EMIT clientRemoved(client);
  client->deleteLater();
}
