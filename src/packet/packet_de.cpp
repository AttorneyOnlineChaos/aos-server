#include "aoclient.h"

#include "area_data.h"
#include "server.h"

void kenji::AOClient::process(const theory::DeleteEvidencePacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (!checkEvidenceAccess(l_area))
  {
    return;
  }

  int l_idx = packet.evidenceId;
  if (l_idx < l_area->evidence().size() && l_idx >= 0)
  {
    l_area->deleteEvidence(l_idx);
  }
  sendEvidenceList(l_area);
}
