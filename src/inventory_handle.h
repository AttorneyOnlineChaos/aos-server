#pragma once

#include "game/game_defs.h"

namespace kenji
{
class InventoryHandle
{
public:
  virtual ~InventoryHandle() = default;

  virtual theory::InventoryPermission permission(theory::PlayerId playerId) const = 0;
  virtual int capacity() const = 0;
};
} // namespace kenji
