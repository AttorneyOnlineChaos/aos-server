#include "ao_client.h"

void kenji::AOClient::process(const theory::InventoryTransferPacket &packet)
{
  if (m_inventories.hasPermission(packet.inventoryId, id) < theory::InventoryPermission::Edit)
  {
    shipGameError(theory::GameError::inventoryAccessDenied(QString::number(packet.inventoryId)));
    return;
  }

  const int limit = m_inventories.capacity(packet.inventoryId);
  int held = 0;
  if (packet.mode == theory::InventoryTransferPacket::Append)
  {
    held = m_inventories.count(packet.inventoryId);
  }
  if (held + packet.list.size() > limit)
  {
    shipGameError(theory::GameError::inventoryFull(QString::number(limit)));
    return;
  }

  if (packet.mode == theory::InventoryTransferPacket::Replace)
  {
    m_inventories.resetEvidence(packet.inventoryId, packet.list);
    return;
  }
  for (const theory::Evidence &evidence : packet.list)
  {
    m_inventories.createEvidence(packet.inventoryId, evidence);
  }
}
