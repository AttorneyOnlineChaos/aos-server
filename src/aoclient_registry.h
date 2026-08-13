#pragma once

#include "aoclient.h"

#include "core/pointer_types.h"
#include "network/cargo_socket.h"

#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStack>
#include <QString>

#include <functional>

namespace kenji
{
class AOClientRegistry : public QObject
{
  Q_OBJECT

public:
  AOClientRegistry(Server &server, ULogger &logger, MusicManager &musicManager, int capacity, QObject *parent = nullptr);

  int capacity() const;
  int count() const;

  AOClient *create(const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address);
  void remove(AOClient *client);

  AOClient *client(int id) const;
  QList<AOClient *> clients() const;

  using Condition = std::function<bool(const AOClient *)>;
  QList<AOClient *> clientsIf(const Condition &condition) const;
  int countIf(const Condition &condition) const;

  QList<AOClient *> clientsByIpid(const QString &ipid) const;
  QList<AOClient *> clientsByHwid(const QString &hwid) const;
  int countByAddress(const QHostAddress &address) const;

Q_SIGNALS:
  void clientAdded(AOClient *client);
  void clientRemoved(AOClient *client);

private:
  Server &_server;
  ULogger &_logger;
  MusicManager &_musicManager;

  int _capacity = 0;
  QStack<int> _availableIds;
  QMap<int, AOClient *> _clients;
};
} // namespace kenji
