#include "ao_client_registry.h"

#include "inventory/client_inventory_handle.h"

kenji::AOClientRegistry::AOClientRegistry(Server &server, ULogger &logger, MusicManager &musicManager, InventoryRegistry &inventories, int capacity, QObject *parent)
    : QObject{parent}
    , _server{server}
    , _logger{logger}
    , _musicManager{musicManager}
    , _inventories{inventories}
    , _capacity{capacity}
{}

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

kenji::AOClient *kenji::AOClientRegistry::client(theory::PlayerId id) const
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

QList<kenji::AOClient *> kenji::AOClientRegistry::clientsInArea(theory::AreaId areaId) const
{
  return clientsIf([areaId](const AOClient *client) { return client->areaId() == areaId; });
}

int kenji::AOClientRegistry::countByAddress(const QHostAddress &address) const
{
  return countIf([&address](const AOClient *client) { return address.isEqual(client->m_remote_ip); });
}

kenji::AOClient *kenji::AOClientRegistry::create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address)
{
  if (_clients.size() >= _capacity)
  {
    return nullptr;
  }

  const theory::PlayerId id = _ids.acquire();
  const theory::InventoryId inventoryId = _inventories.add(theory::makeShared<ClientInventoryHandle>(id, _server));
  AOClient *client = new AOClient(&_server, _logger, _inventories, socket, address, this, id, inventoryId, &_musicManager);
  _clients.insert(id, client);
  Q_EMIT clientAdded(id);
  return client;
}

void kenji::AOClientRegistry::remove(AOClient *client)
{
  if (!client || _clients.value(client->id) != client)
  {
    return;
  }

  Q_EMIT aboutToRemoveClient(client->id);
  _clients.remove(client->id);
  _inventories.remove(client->inventoryId);
  _ids.release(client->id);
  Q_EMIT clientRemoved(client->id);
  client->deleteLater();
}
