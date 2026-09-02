#include "inventory/area_inventory_handle.h"

#include "config_manager.h"

kenji::AreaInventoryHandle::AreaInventoryHandle(theory::AreaId area, Server &server)
    : _area{area}
    , _server{server}
{}

theory::InventoryPermission kenji::AreaInventoryHandle::permission(theory::PlayerId playerId) const
{
  if (_server.getAreaById(_area)->owners().contains(playerId))
  {
    return theory::InventoryPermission::Edit;
  }
  return theory::InventoryPermission::View;
}

int kenji::AreaInventoryHandle::capacity() const
{
  return ConfigManager::maxInventorySize();
}
