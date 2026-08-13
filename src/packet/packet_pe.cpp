#include "aoclient.h"

#include "area_data.h"
#include "server.h"

#include <QRegularExpression>

void kenji::AOClient::process(const theory::AddEvidencePacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (!checkEvidenceAccess(l_area))
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

  l_area->appendEvidence(evidence);
  sendEvidenceList(l_area);
}
