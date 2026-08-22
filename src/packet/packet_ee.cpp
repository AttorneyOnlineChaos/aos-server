#include "ao_client.h"

#include "area_data.h"
#include "server.h"

#include <QRegularExpression>

void kenji::AOClient::process(const theory::EditEvidencePacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (!checkEvidenceAccess(l_area))
  {
    return;
  }

  int l_evi_id = packet.evidenceId;
  if (l_evi_id >= l_area->evidence().length() || l_evi_id < 0)
  {
    return;
  }

  QString description = packet.evidence.description;

  // Automatically add <owner=all> for evidence in HIDDEN_CM mode areas
  if (l_area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM)
  {
    // Check if owner tag already exists in description
    static const QRegularExpression ownerRegex("<owner=(.*?)>");
    if (!ownerRegex.match(description).hasMatch())
    {
      // Add <owner=all> at the beginning if no owner tag exists
      description = "<owner=all>\n" + description;
    }
  }

  AreaData::Evidence evidence;
  evidence.name = packet.evidence.name;
  evidence.description = description;
  evidence.image = packet.evidence.image;

  l_area->replaceEvidence(l_evi_id, evidence);
  sendEvidenceList(l_area);
}
