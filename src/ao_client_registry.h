#pragma once

#include "ao_client.h"
#include "core/pointer_types.h"
#include "inventory_registry.h"
#include "network/cargo_socket.h"

#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <functional>

namespace kenji
{
class AOClientRegistry : public QObject
{
  Q_OBJECT

public:
  AOClientRegistry(Server &server, ULogger &logger, MusicManager &musicManager, InventoryRegistry &inventories, int capacity, QObject *parent = nullptr);

  int capacity() const;
  int count() const;

  AOClient *create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address);
  void remove(AOClient *client);

  AOClient *client(theory::PlayerId id) const;
  QList<AOClient *> clients() const;

  using Condition = std::function<bool(const AOClient *)>;
  QList<AOClient *> clientsIf(const Condition &condition) const;
  int countIf(const Condition &condition) const;

  QList<AOClient *> clientsByIpid(const QString &ipid) const;
  QList<AOClient *> clientsByHwid(const QString &hwid) const;
  QList<AOClient *> clientsInArea(theory::AreaId areaId) const;
  int countByAddress(const QHostAddress &address) const;

Q_SIGNALS:
  void clientAdded(theory::PlayerId playerId);
  void aboutToRemoveClient(theory::PlayerId playerId);
  void clientRemoved(theory::PlayerId playerId);

private:
  Server &_server;
  ULogger &_logger;
  MusicManager &_musicManager;
  InventoryRegistry &_inventories;

  int _capacity = 0;
  theory::IdCounter _ids;
  QMap<theory::PlayerId, AOClient *> _clients;
};
} // namespace kenji
