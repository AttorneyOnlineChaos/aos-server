#include "inventory/client_inventory_handle.h"

#include "config_manager.h"

kenji::ClientInventoryHandle::ClientInventoryHandle(theory::PlayerId owner, const Server &server)
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
  return theory::InventoryPermission::View;
}

int kenji::ClientInventoryHandle::capacity() const
{
  return ConfigManager::maxPersonalInventorySize();
}
