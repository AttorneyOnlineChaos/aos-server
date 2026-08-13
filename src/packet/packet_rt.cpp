#include "aoclient.h"

#include "area_data.h"
#include "server.h"

#include <QDateTime>

void kenji::AOClient::process(const theory::SplashPacket &packet)
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

  if (!l_area->isWtceAllowed())
  {
    sendServerMessage("WTCE animations have been disabled in this area.");
    return;
  }

  if (QDateTime::currentDateTime().toSecsSinceEpoch() - m_last_wtce_time <= 5)
  {
    return;
  }
  m_last_wtce_time = QDateTime::currentDateTime().toSecsSinceEpoch();
  server->broadcastToArea(packet, areaId());
  updateJudgeLog(l_area, this, "WT/CE");
}
