#pragma once

#include "game/game_defs.h"
#include "inventory_handle.h"
#include "server.h"

namespace kenji
{
class AreaInventoryHandle : public InventoryHandle
{
public:
  AreaInventoryHandle(theory::AreaId area, Server &server);

  theory::InventoryPermission permission(theory::PlayerId playerId) const override;
  int capacity() const override;

private:
  theory::AreaId _area;
  Server &_server;
};
} // namespace kenji
