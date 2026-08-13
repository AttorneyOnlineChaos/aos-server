#include "session_registry.h"

#include "config_manager.h"

#include "protocol/session_tools.h"

#include <QPointer>

kenji::SessionRegistry::SessionRegistry(AOClientRegistry &clients, QObject *parent)
    : QObject{parent}
    , _clients{clients}
{}

std::optional<kenji::SessionRegistry::Ticket> kenji::SessionRegistry::join(const std::optional<QString> &sessionToken, const QString &hwid, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &address)
{
  if (sessionToken)
  {
    const QString tokenKey = theory::sessionComputeKey({sessionToken.value(), hwid});
    if (const auto it = _sessions.constFind(tokenKey); it != _sessions.constEnd() && it.value()->getHwid() == hwid && it.value()->sessionStatus() != AOClient::SessionStatus::Expired)
    {
      Ticket ticket;
      ticket.token = theory::sessionGenerateToken();
      ticket.client = it.value();
      ticket.recovered = true;
      _sessions.remove(tokenKey);
      _sessions.insert(theory::sessionComputeKey({ticket.token, hwid}), ticket.client);
      return ticket;
    }
  }

  if (_clients.countByAddress(address) + 1 > ConfigManager::multiClientLimit() && !address.isLoopback())
  {
    return std::nullopt;
  }

  AOClient *client = _clients.create(socket, address);
  if (!client)
  {
    return std::nullopt;
  }
  client->m_hwid = hwid;

  // NOTE: queued is required; the client must not be destroyed mid-emission
  connect(client, &AOClient::sessionStatusChanged, this, [this, client](AOClient::SessionStatus status) {
    if (status == AOClient::SessionStatus::Expired)
    {
      remove(client);
    }
  }, Qt::QueuedConnection);

  Ticket ticket;
  ticket.token = theory::sessionGenerateToken();
  ticket.client = client;
  _sessions.insert(theory::sessionComputeKey({ticket.token, hwid}), client);
  return ticket;
}

void kenji::SessionRegistry::remove(AOClient *client)
{
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
