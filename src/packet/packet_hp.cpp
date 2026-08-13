#include "aoclient.h"

#include "area_data.h"
#include "server.h"

void kenji::AOClient::process(const theory::PenaltyPacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (isSpectator())
  {
    sendServerMessage("Spectators are blocked from using the judge controls.");
    return;
  }

  if (l_area->lockStatus() == theory::AreaLockStatus::Spectatable && !l_area->invited().contains(clientId()) && !checkPermission(ACLRole::BYPASS_LOCKS))
  {
    sendServerMessage("Spectators are blocked from using the judge controls.");
    return;
  }

  if (m_is_wtce_blocked)
  {
    sendServerMessage("You are blocked from using the judge controls.");
    return;
  }

  switch (packet.bar)
  {
  default:
    drop(theory::ErrorPacket::ProtocolError, "Packet : penalty");
    return;
  case theory::HealthBar::Defense:
    l_area->changeHP(AreaData::Side::DEFENCE, packet.value);
    break;
  case theory::HealthBar::Prosecution:
    l_area->changeHP(AreaData::Side::PROSECUTOR, packet.value);
    break;
  }

  theory::PenaltyPacket l_def_penalty;
  l_def_penalty.bar = theory::HealthBar::Defense;
  l_def_penalty.value = l_area->defHP();
  server->broadcastToArea(l_def_penalty, l_area->index());

  theory::PenaltyPacket l_pro_penalty;
  l_pro_penalty.bar = theory::HealthBar::Prosecution;
  l_pro_penalty.value = l_area->proHP();
  server->broadcastToArea(l_pro_penalty, l_area->index());

  updateJudgeLog(l_area, this, "updated the penalties");
}
