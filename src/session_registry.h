#pragma once

#include "ao_client_registry.h"

#include "core/pointer_types.h"
#include "network/cargo_socket.h"

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QString>

#include <optional>

namespace kenji
{
class SessionRegistry : public QObject
{
  Q_OBJECT

public:
  explicit SessionRegistry(AOClientRegistry &clients, QObject *parent = nullptr);

  struct Ticket
  {
    QString token;
    AOClient *client = nullptr;
    bool recovered = false;
  };
  std::optional<Ticket> join(const std::optional<QString> &sessionToken, const QString &hwid, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address);

private:
  AOClientRegistry &_clients;
  QHash<QString, AOClient *> _sessions;

  void remove(AOClient *client);
};
} // namespace kenji
