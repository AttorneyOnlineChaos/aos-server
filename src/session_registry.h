#pragma once

#include "ao_client_registry.h"
#include "badge/badge_defs.h"
#include "badge/guest_token_registry.h"
#include "badge/user_database.h"
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
  SessionRegistry(AOClientRegistry &clients, theory::GuestTokenRegistry &guests, QObject *parent = nullptr);

  struct Ticket
  {
    QString token;
    AOClient *client = nullptr;
    bool recovered = false;
  };
  std::optional<Ticket> join(const theory::UserDatabase::Ticket &admission, const std::optional<QString> &sessionToken, const QString &hwid, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address);

private:
  AOClientRegistry &_clients;
  theory::GuestTokenRegistry &_guests;
  QHash<QString, AOClient *> _sessions;
  QHash<AOClient *, QString> _guestTokens;

  void remove(AOClient *client);
};
} // namespace kenji
