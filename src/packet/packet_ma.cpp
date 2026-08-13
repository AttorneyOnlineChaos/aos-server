#include "aoclient.h"

#include "config_manager.h"
#include "db_manager.h"
#include "server.h"

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

  AOClient *target = server->getClientByID(packet.targetClientId);
  if (target == nullptr)
  {
    sendServerMessage("User not found.");
    return;
  }

  QString moderator_name;
  if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED)
  {
    moderator_name = m_moderator_name;
  }
  else
  {
    moderator_name = "Moderator";
  }

  QList<AOClient *> clients = server->getClientsByIpid(target->m_ipid);
  if (is_kick)
  {
    theory::ErrorPacket l_kicked;
    l_kicked.code = theory::ErrorPacket::Banned;
    l_kicked.what = packet.reason;
    for (AOClient *subclient : clients)
    {
      subclient->shipPacket(l_kicked);
      subclient->drop();
    }

    m_logger.logKick(moderator_name, target->m_ipid);

    sendServerMessage("Kicked " + QString::number(clients.size()) + " client(s) with ipid " + target->m_ipid + " for reason: " + packet.reason);
  }
  else
  {
    BanInfo ban;

    ban.ip = target->m_remote_ip;
    ban.ipid = target->m_ipid;
    ban.moderator = moderator_name;
    ban.reason = packet.reason;
    ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();

    if (packet.durationSeconds == -1)
    {
      ban.duration = PermanentBanDuration;
    }
    else
    {
      ban.duration = packet.durationSeconds;
    }
    const QString timestamp = ban.until();

    theory::ErrorPacket l_banned;
    l_banned.code = theory::ErrorPacket::Banned;
    l_banned.what = packet.reason;
    for (AOClient *subclient : clients)
    {
      ban.hdid = subclient->m_hwid;

      server->getDatabaseManager()->addBan(ban);

      subclient->shipPacket(l_banned);
      subclient->drop();
    }

    m_logger.logBan(moderator_name, target->m_ipid, timestamp);

    sendServerMessage("Banned " + QString::number(clients.size()) + " client(s) with ipid " + target->m_ipid + " for reason: " + packet.reason);

    int ban_id = server->getDatabaseManager()->getBanID(ban.ip);
    if (ConfigManager::discordBanWebhookEnabled())
    {
      Q_EMIT server->banWebhookRequest(ban.ipid, ban.moderator, timestamp, ban.reason, ban_id);
    }
  }
}
