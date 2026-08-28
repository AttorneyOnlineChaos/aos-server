#include "ao_client.h"

#include "area_data.h"
#include "server.h"

void kenji::AOClient::process(const theory::DeleteEvidencePacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (!checkEvidenceAccess(l_area))
  {
    return;
  }

  theory::EvidenceId l_evi_id = packet.evidenceId;
  if (l_evi_id < l_area->evidence().size() && l_evi_id >= 0)
  {
    l_area->deleteEvidence(l_evi_id);
  }
  sendEvidenceList(l_area);
}
