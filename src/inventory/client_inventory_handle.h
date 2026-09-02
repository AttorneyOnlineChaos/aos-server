#pragma once

#include "game/game_defs.h"
#include "inventory_handle.h"
#include "server.h"

namespace kenji
{
class ClientInventoryHandle : public InventoryHandle
{
public:
  ClientInventoryHandle(theory::PlayerId owner, const Server &server);

  theory::InventoryPermission permission(theory::PlayerId playerId) const override;
  int capacity() const override;

private:
  theory::PlayerId _owner;
  const Server &_server;
};
} // namespace kenji
