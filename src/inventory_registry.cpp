#include "inventory_registry.h"

kenji::InventoryRegistry::InventoryRegistry(QObject *parent)
    : QObject{parent}
{}

theory::InventoryId kenji::InventoryRegistry::add(const theory::Shared<InventoryHandle> &handle)
{
  const theory::InventoryId inventoryId = _ids.acquire();
  _inventories.insert(inventoryId, Entry{{}, handle});
  Q_EMIT added(inventoryId);
  return inventoryId;
}

void kenji::InventoryRegistry::remove(theory::InventoryId inventoryId)
{
  Q_EMIT aboutToRemove(inventoryId);
  const Entry entry = _inventories.take(inventoryId);
  for (const theory::EvidenceId evidenceId : entry.items)
  {
    discard(evidenceId);
  }
  Q_EMIT removed(inventoryId);
  _ids.release(inventoryId);
}

bool kenji::InventoryRegistry::contains(theory::InventoryId inventoryId) const
{
  return _inventories.contains(inventoryId);
}

QList<theory::InventoryId> kenji::InventoryRegistry::inventories() const
{
  return _inventories.keys();
}

int kenji::InventoryRegistry::count(theory::InventoryId inventoryId) const
{
  return _inventories.value(inventoryId).items.size();
}

int kenji::InventoryRegistry::capacity(theory::InventoryId inventoryId) const
{
  const auto entry = _inventories.constFind(inventoryId);
  if (entry == _inventories.constEnd())
  {
    return 0;
  }
  return entry->handle->capacity();
}

QList<theory::EvidenceItem> kenji::InventoryRegistry::inventory(theory::InventoryId inventoryId) const
{
  const QList<theory::EvidenceId> contents = _inventories.value(inventoryId).items;
  QList<theory::EvidenceItem> held;
  held.reserve(contents.size());
  for (const theory::EvidenceId evidenceId : contents)
  {
    held.append(theory::EvidenceItem{evidenceId, _evidence.value(evidenceId)});
  }
  return held;
}

theory::InventoryPermission kenji::InventoryRegistry::hasPermission(theory::InventoryId inventoryId, theory::PlayerId playerId) const
{
  const auto entry = _inventories.constFind(inventoryId);
  if (entry == _inventories.constEnd())
  {
    return theory::InventoryPermission::NoPermission;
  }
  return entry->handle->permission(playerId);
}

std::optional<theory::InventoryId> kenji::InventoryRegistry::inventoryOf(theory::EvidenceId evidenceId) const
{
  const auto owner = _owners.constFind(evidenceId);
  if (owner == _owners.constEnd())
  {
    return std::nullopt;
  }
  return owner.value();
}

std::optional<theory::EvidenceItem> kenji::InventoryRegistry::evidence(theory::EvidenceId evidenceId) const
{
  const auto held = _evidence.constFind(evidenceId);
  if (held == _evidence.constEnd())
  {
    return std::nullopt;
  }
  return theory::EvidenceItem{evidenceId, held.value()};
}

theory::EvidenceId kenji::InventoryRegistry::createEvidence(theory::InventoryId inventoryId, const theory::Evidence &asset)
{
  const theory::EvidenceId evidenceId = create(inventoryId, asset);
  Q_EMIT addedEvidence(inventoryId, theory::EvidenceItem{evidenceId, asset});
  return evidenceId;
}

bool kenji::InventoryRegistry::removeEvidence(theory::EvidenceId evidenceId)
{
  const auto owner = inventoryOf(evidenceId);
  if (!owner)
  {
    return false;
  }
  Q_EMIT aboutToRemoveEvidence(owner.value(), theory::EvidenceItem{evidenceId, _evidence.value(evidenceId)});
  _inventories[owner.value()].items.removeOne(evidenceId);
  discard(evidenceId);
  Q_EMIT removedEvidence(evidenceId);
  return true;
}

void kenji::InventoryRegistry::resetEvidence(theory::InventoryId inventoryId, const QList<theory::Evidence> &assets)
{
  const auto contents = _inventories.find(inventoryId);
  const QList<theory::EvidenceId> previous = contents->items;
  for (const theory::EvidenceId evidenceId : previous)
  {
    discard(evidenceId);
  }
  contents->items.clear();

  for (const theory::Evidence &asset : assets)
  {
    create(inventoryId, asset);
  }
  Q_EMIT evidenceReset(inventoryId);
}

void kenji::InventoryRegistry::clear(theory::InventoryId inventoryId)
{
  const auto contents = _inventories.find(inventoryId);
  if (contents->items.isEmpty())
  {
    return;
  }

  const QList<theory::EvidenceId> previous = contents->items;
  for (const theory::EvidenceId evidenceId : previous)
  {
    discard(evidenceId);
  }
  contents->items.clear();
  Q_EMIT evidenceReset(inventoryId);
}

bool kenji::InventoryRegistry::setEvidence(theory::EvidenceId evidenceId, const theory::Evidence &asset)
{
  const auto held = _evidence.find(evidenceId);
  if (held == _evidence.end())
  {
    return false;
  }
  if (held.value() != asset)
  {
    const theory::Evidence previous = held.value();
    held.value() = asset;
    Q_EMIT evidenceReplaced(_owners.value(evidenceId), theory::EvidenceItem{evidenceId, asset}, previous);
  }
  return true;
}

bool kenji::InventoryRegistry::setEvidenceRevealed(theory::EvidenceId evidenceId, bool revealed)
{
  const auto held = _evidence.constFind(evidenceId);
  if (held == _evidence.constEnd())
  {
    return false;
  }
  theory::Evidence asset = held.value();
  asset.revealed = revealed;
  return setEvidence(evidenceId, asset);
}

theory::EvidenceId kenji::InventoryRegistry::create(theory::InventoryId inventoryId, const theory::Evidence &asset)
{
  const auto contents = _inventories.find(inventoryId);
  const theory::EvidenceId evidenceId = _ids.acquire();
  _evidence.insert(evidenceId, asset);
  _owners.insert(evidenceId, inventoryId);
  contents->items.append(evidenceId);
  return evidenceId;
}

void kenji::InventoryRegistry::discard(theory::EvidenceId evidenceId)
{
  _evidence.remove(evidenceId);
  _owners.remove(evidenceId);
  _ids.release(evidenceId);
}
