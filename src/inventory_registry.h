#pragma once

#include "core/pointer_types.h"
#include "game/evidence.h"
#include "game/game_defs.h"
#include "inventory_handle.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace kenji
{
class InventoryRegistry : public QObject
{
  Q_OBJECT

public:
  explicit InventoryRegistry(QObject *parent = nullptr);

  theory::InventoryId add(const theory::Shared<InventoryHandle> &handle);
  void remove(theory::InventoryId inventoryId);
  bool contains(theory::InventoryId inventoryId) const;
  QList<theory::InventoryId> inventories() const;
  int count(theory::InventoryId inventoryId) const;
  int capacity(theory::InventoryId inventoryId) const;
  QList<theory::EvidenceItem> inventory(theory::InventoryId inventoryId) const;
  theory::InventoryPermission hasPermission(theory::InventoryId inventoryId, theory::PlayerId playerId) const;

  std::optional<theory::InventoryId> inventoryOf(theory::EvidenceId evidenceId) const;
  std::optional<theory::EvidenceItem> evidence(theory::EvidenceId evidenceId) const;
  theory::EvidenceId createEvidence(theory::InventoryId inventoryId, const theory::Evidence &asset);
  bool removeEvidence(theory::EvidenceId evidenceId);
  void resetEvidence(theory::InventoryId inventoryId, const QList<theory::Evidence> &assets);
  void clear(theory::InventoryId inventoryId);

  bool setEvidence(theory::EvidenceId evidenceId, const theory::Evidence &asset);
  bool setEvidenceRevealed(theory::EvidenceId evidenceId, bool revealed);

Q_SIGNALS:
  void added(theory::InventoryId inventoryId);
  void aboutToRemove(theory::InventoryId inventoryId);
  void removed(theory::InventoryId inventoryId);

  void addedEvidence(theory::InventoryId inventoryId, const theory::EvidenceItem &item);
  void aboutToRemoveEvidence(theory::InventoryId inventoryId, const theory::EvidenceItem &item);
  void removedEvidence(theory::EvidenceId evidenceId);
  void evidenceReset(theory::InventoryId inventoryId);
  void evidenceReplaced(theory::InventoryId inventoryId, const theory::EvidenceItem &item, const theory::Evidence &previous);

private:
  struct Entry
  {
    QList<theory::EvidenceId> items;
    theory::Shared<InventoryHandle> handle;
  };

  theory::IdCounter _ids;
  QHash<theory::InventoryId, Entry> _inventories;
  QHash<theory::EvidenceId, theory::Evidence> _evidence;
  QHash<theory::EvidenceId, theory::InventoryId> _owners;

  theory::EvidenceId create(theory::InventoryId inventoryId, const theory::Evidence &asset);
  void discard(theory::EvidenceId evidenceId);
};
} // namespace kenji
