#include "ao_client.h"

#include "core/json_codec.h"

void kenji::AOClient::process(const theory::EvidenceUpdatePacket &packet)
{
  const auto owner = m_inventories.inventoryOf(packet.evidenceId);
  if (!owner || m_inventories.hasPermission(owner.value(), id) < theory::InventoryPermission::Edit)
  {
    shipGameError(theory::GameError::invalidEvidence(QString::number(packet.evidenceId)));
    return;
  }

  switch (packet.property)
  {
  default:
    drop(theory::ErrorPacket::ProtocolError);
    break;
  case theory::EvidenceUpdatePacket::Snapshot:
    {
      theory::Evidence asset;
      if (theory::decodeJson(packet.data, asset))
      {
        drop(theory::ErrorPacket::ProtocolError);
        break;
      }
      m_inventories.setEvidence(packet.evidenceId, asset);
      break;
    }
  }
}
