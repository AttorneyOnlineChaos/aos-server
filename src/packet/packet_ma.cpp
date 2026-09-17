#include "ao_client.h"

#include "config_manager.h"
#include "server.h"
#include "server_database.h"

#include <QDateTime>

void kenji::AOClient::process(const theory::ModActionPacket &packet)
{
  if (!m_authenticated)
  {
    sendServerMessage("You are not logged in!");
    return;
  }

  bool is_kick;
  switch (packet.action)
  {
  default:
    return;
  case theory::ModActionPacket::Kick:
    is_kick = true;
    break;
  case theory::ModActionPacket::Ban:
    is_kick = false;
    break;
  }

  if (is_kick)
  {
    if (!checkPermission(ACLRole::KICK))
    {
      sendServerMessage("You do not have permission to kick users.");
      return;
    }
  }
  else
  {
    if (!checkPermission(ACLRole::BAN))
    {
      sendServerMessage("You do not have permission to ban users.");
      return;
    }
  }

  AOClient *target = server->getClientByID(packet.targetPlayerId);
  if (target == nullptr)
  {
    sendServerMessage("User not found.");
    return;
  }

  const theory::UserId l_user_id = target->userId;

  if (is_kick)
  {
    QList<AOClient *> clients;
    if (target->isGuest())
    {
      clients = {target};
    }
    else
    {
      clients = server->getClientsByUserId(l_user_id);
    }

    theory::ErrorPacket l_kicked;
    l_kicked.code = theory::ErrorPacket::Banned;
    l_kicked.what = packet.reason;
    for (AOClient *subclient : clients)
    {
      subclient->shipPacket(l_kicked);
      subclient->drop();
    }

    m_logger.logKick(userId, l_user_id);
    sendServerMessage("Kicked " + QString::number(clients.size()) + " client(s) for reason: " + packet.reason);
  }
  else
  {
    if (target->isGuest())
    {
      sendServerMessage("Guests can't be banned, only kicked.");
      return;
    }

    theory::BanRecord ban;
    ban.subjectId = l_user_id;
    ban.issuerId = userId;
    ban.reason = packet.reason;
    ban.issuedOn = QDateTime::currentSecsSinceEpoch();
    if (packet.durationSeconds == -1)
    {
      ban.duration = theory::PermanentBanDuration;
    }
    else
    {
      ban.duration = packet.durationSeconds;
    }

    const std::optional<theory::BanId> ban_id = server->database().addBan(ban);
    if (!ban_id)
    {
      sendServerMessage("The ban could not be recorded.");
      return;
    }

    const QString timestamp = ban.until();

    const QList<AOClient *> clients = server->getClientsByUserId(l_user_id);
    theory::ErrorPacket l_banned;
    l_banned.code = theory::ErrorPacket::Banned;
    l_banned.what = packet.reason;
    for (AOClient *subclient : clients)
    {
      subclient->shipPacket(l_banned);
      subclient->drop();
    }

    m_logger.logBan(userId, l_user_id, timestamp);
    sendServerMessage("Banned " + QString::number(clients.size()) + " client(s) for reason: " + packet.reason);

    if (ConfigManager::discordBanWebhookEnabled())
    {
      Q_EMIT server->banWebhookRequest(l_user_id, name(), timestamp, ban.reason, ban_id.value());
    }
  }
}
