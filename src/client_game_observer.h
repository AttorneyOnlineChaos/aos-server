#pragma once

#include "ao_client.h"
#include "ao_client_registry.h"
#include "area_data.h"
#include "game/evidence.h"
#include "game/game_defs.h"
#include "inventory_registry.h"
#include "protocol/packets/area_packets.h"
#include "protocol/packets/evidence_packets.h"
#include "protocol/packets/roster_packets.h"

#include <QHash>
#include <QObject>
#include <QSet>

namespace kenji
{
class Server;

class ClientGameObserver : public QObject
{
  Q_OBJECT

public:
  ClientGameObserver(AOClient &viewer, AOClientRegistry &clients, InventoryRegistry &inventories, Server &server, QObject *parent = nullptr);

  bool isEvidenceVisible(theory::InventoryId inventoryId, const theory::Evidence &evidence) const;

private:
  AOClient &_viewer;
  AOClientRegistry &_clients;
  InventoryRegistry &_inventories;
  Server &_server;

  QHash<theory::InventoryId, QSet<theory::EvidenceId>> _shippedEvidence;

  void connectClient(AOClient *client);
  void shipPlayerRecord(theory::PlayerId playerId, theory::PlayerRecordPacket::Action action);
  void shipPlayerUpdate(const AOClient &client, theory::PlayerUpdatePacket::Property property);
  void shipPlayerUpdates(const AOClient &client);

  void shipAreaRecord(const AreaData &area);
  void shipAreaUpdate(const AreaData &area, theory::AreaUpdatePacket::Property property);
  void shipAreaUpdates(const AreaData &area);

  bool isInventoryReachable(theory::InventoryId inventoryId) const;
  void shipInventoryRecord(theory::InventoryId inventoryId, theory::InventoryRecordPacket::Action action);
  void shipInventoryUpdate(theory::InventoryId inventoryId);

  void shipEvidenceRecord(theory::InventoryId inventoryId, theory::EvidenceId evidenceId, theory::EvidenceRecordPacket::Action action);
  void shipEvidenceUpdate(const theory::EvidenceItem &item);
  void synchronizeEvidence(theory::InventoryId inventoryId, const theory::EvidenceItem &item);
  void synchronizeAllEvidence();

  void shipSnapshot();
};
} // namespace kenji
