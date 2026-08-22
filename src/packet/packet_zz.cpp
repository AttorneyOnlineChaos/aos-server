#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

void kenji::AOClient::process(const theory::ModCallPacket &packet)
{
  if (packet.reason.length() > ConfigManager::modcallReasonLimit())
  {
    sendServerMessage("Your modcall reason is too long! Please limit it to " + QString::number(ConfigManager::modcallReasonLimit()) + " characters.");
    return;
  }

  QString l_name = name();
  if (l_name.isEmpty())
  {
    l_name = character();
  }

  QString l_areaName = server->getAreaById(areaId())->name();
  QString l_id = QString::number(clientId());

  theory::ModCallNoticePacket l_notice;
  l_notice.area = l_areaName;
  l_notice.callerClientId = clientId();
  l_notice.callerName = l_name;
  l_notice.reason = packet.reason;

  if (packet.targetClientId != theory::NoClientId)
  {
    AOClient *target = server->getClientByID(packet.targetClientId);
    if (target)
    {
      l_notice.targetName = target->name();
    }
  }

  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->m_authenticated)
    {
      l_client->shipPacket(l_notice);
    }
  }
  m_logger.logModcall(l_areaName, m_ipid, name(), QString::number(clientId()), (character() + " " + characterName().value_or(QString())));

  if (ConfigManager::discordModcallWebhookEnabled())
  {
    QString webhook_reason = packet.reason;
    if (packet.targetClientId != theory::NoClientId)
    {
      AOClient *target = server->getClientByID(packet.targetClientId);
      if (target)
      {
        webhook_reason.append(" (Regarding: " + target->name() + ")");
      }
    }

    Q_EMIT server->modcallWebhookRequest(l_name, l_areaName, l_id, webhook_reason, server->getAreaBuffer(l_areaName));
  }
}
