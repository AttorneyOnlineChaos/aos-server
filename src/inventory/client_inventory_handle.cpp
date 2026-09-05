#include "inventory/client_inventory_handle.h"

#include "ao_client.h"
#include "area_data.h"
#include "config_manager.h"

kenji::ClientInventoryHandle::ClientInventoryHandle(theory::PlayerId owner, Server &server)
    : _owner{owner}
    , _server{server}
{}

theory::InventoryPermission kenji::ClientInventoryHandle::permission(theory::PlayerId playerId) const
{
  if (!_server.personalInventoriesEnabled())
  {
    return theory::InventoryPermission::NoPermission;
  }
  if (playerId == _owner)
  {
    return theory::InventoryPermission::Edit;
  }
  // TODO: this is a temporary solution, once parties are rolling out this new rule must be removed
  AOClient *owner = _server.getClientByID(_owner);
  AOClient *viewer = _server.getClientByID(playerId);
  if (!owner || !viewer || owner->areaId() != viewer->areaId())
  {
    return theory::InventoryPermission::NoPermission;
  }
  AreaData *area = _server.getAreaById(owner->areaId());
  if (area && area->owners().contains(playerId))
  {
    return theory::InventoryPermission::View;
  }
  return theory::InventoryPermission::NoPermission;
}

int kenji::ClientInventoryHandle::capacity() const
{
  return ConfigManager::maxPersonalInventorySize();
}
