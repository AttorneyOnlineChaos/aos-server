#include "session_registry.h"

#include "config_manager.h"
#include "core/token_tools.h"

#include <QPointer>

kenji::SessionRegistry::SessionRegistry(AOClientRegistry &clients, theory::GuestTokenRegistry &guests, QObject *parent)
    : QObject{parent}
    , _clients{clients}
    , _guests{guests}
{}

std::optional<kenji::SessionRegistry::Ticket> kenji::SessionRegistry::join(const theory::UserDatabase::Ticket &admission, const std::optional<QString> &sessionToken, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address)
{
  if (sessionToken)
  {
    const QString tokenKey = theory::computeTokenKey({sessionToken.value()});
    if (const auto it = _sessions.constFind(tokenKey); it != _sessions.constEnd() && it.value()->userId == admission.user.id && it.value()->sessionStatus() != AOClient::SessionStatus::Expired)
    {
      Ticket ticket;
      ticket.token = theory::generateToken();
      ticket.client = it.value();
      ticket.recovered = true;
      _sessions.remove(tokenKey);
      _sessions.insert(theory::computeTokenKey({ticket.token}), ticket.client);
      if (admission.user.id == theory::NoUserId)
      {
        _guestTokens.insert(ticket.client, admission.token);
      }

      return ticket;
    }
  }

  if (_clients.countByAddress(address) + 1 > ConfigManager::multiClientLimit() && !address.isLoopback())
  {
    return std::nullopt;
  }

  AOClient *client = _clients.create(socket, address, admission.user.id);
  if (!client)
  {
    return std::nullopt;
  }

  if (admission.user.id == theory::NoUserId)
  {
    _guestTokens.insert(client, admission.token);
  }

  // NOTE: queued is required; the client must not be destroyed mid-emission
  connect(client, &AOClient::sessionStatusChanged, this, [this, client](AOClient::SessionStatus status) {
    if (status == AOClient::SessionStatus::Expired)
    {
      remove(client);
    }
  }, Qt::QueuedConnection);

  Ticket ticket;
  ticket.token = theory::generateToken();
  ticket.client = client;
  _sessions.insert(theory::computeTokenKey({ticket.token}), client);
  return ticket;
}

void kenji::SessionRegistry::dropAll()
{
  const QList<AOClient *> clients = _clients.clients();
  for (AOClient *client : clients)
  {
    client->drop();
  }
}

void kenji::SessionRegistry::remove(AOClient *client)
{
  if (const auto it = _guestTokens.constFind(client); it != _guestTokens.constEnd())
  {
    _guests.revoke(it.value());
    _guestTokens.erase(it);
  }

  for (auto it = _sessions.begin(); it != _sessions.end();)
  {
    if (it.value() == client)
    {
      it = _sessions.erase(it);
    }
    else
    {
      ++it;
    }
  }

  _clients.remove(client);
}
