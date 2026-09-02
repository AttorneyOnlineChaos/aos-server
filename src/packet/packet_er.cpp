#include "ao_client.h"

void kenji::AOClient::process(const theory::EvidenceRecordPacket &packet)
{
  if (packet.action != theory::EvidenceRecordPacket::Remove)
  {
    drop(theory::ErrorPacket::ProtocolError);
    return;
  }

  if (m_inventories.hasPermission(packet.inventoryId, id) < theory::InventoryPermission::Edit)
  {
    shipGameError(theory::GameError::inventoryAccessDenied(QString::number(packet.inventoryId)));
    return;
  }

  const auto owner = m_inventories.inventoryOf(packet.evidenceId);
  if (!owner || owner.value() != packet.inventoryId)
  {
    shipGameError(theory::GameError::invalidEvidence(QString::number(packet.evidenceId)));
    return;
  }

  m_inventories.removeEvidence(packet.evidenceId);
}
