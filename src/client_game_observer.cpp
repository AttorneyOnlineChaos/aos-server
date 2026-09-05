#include "client_game_observer.h"

#include "core/json_codec.h"
#include "server.h"

kenji::ClientGameObserver::ClientGameObserver(AOClient &viewer, AOClientRegistry &clients, InventoryRegistry &inventories, Server &server, QObject *parent)
    : QObject{parent}
    , _viewer{viewer}
    , _clients{clients}
    , _inventories{inventories}
    , _server{server}
{
  for (AOClient *client : _clients.clients())
  {
    connectClient(client);
  }
  connect(&_clients, &AOClientRegistry::clientAdded, this, [this](theory::PlayerId playerId) {
    AOClient *client = _clients.client(playerId);
    connectClient(client);
    shipPlayerRecord(playerId, theory::PlayerRecordPacket::Add);
    shipPlayerUpdates(*client);
  });
  connect(&_clients, &AOClientRegistry::clientRemoved, this, [this](theory::PlayerId playerId) { shipPlayerRecord(playerId, theory::PlayerRecordPacket::Remove); });

  for (AreaData *area : _server.getAreas())
  {
    connect(area, &AreaData::ownersChanged, this, [this, area] {
      shipAreaUpdate(*area, theory::AreaUpdatePacket::Ownership);
      shipInventoryUpdate(area->inventoryId);
      synchronizeAllEvidence();
    });
    connect(area, &AreaData::statusChanged, this, [this, area] { shipAreaUpdate(*area, theory::AreaUpdatePacket::Status); });
    connect(area, &AreaData::lockStatusChanged, this, [this, area] { shipAreaUpdate(*area, theory::AreaUpdatePacket::Locked); });
  }

  connect(&_inventories, &InventoryRegistry::added, this, [this](theory::InventoryId inventoryId) {
    shipInventoryRecord(inventoryId, theory::InventoryRecordPacket::Add);
    shipInventoryUpdate(inventoryId);
  });
  connect(&_inventories, &InventoryRegistry::removed, this, [this](theory::InventoryId inventoryId) {
    _shippedEvidence.remove(inventoryId);
    shipInventoryRecord(inventoryId, theory::InventoryRecordPacket::Remove);
  });
  connect(&_inventories, &InventoryRegistry::addedEvidence, this, &ClientGameObserver::synchronizeEvidence);
  connect(&_inventories, &InventoryRegistry::aboutToRemoveEvidence, this, [this](theory::InventoryId inventoryId, const theory::EvidenceItem &item) {
    if (_shippedEvidence.value(inventoryId).contains(item.id))
    {
      shipEvidenceRecord(inventoryId, item.id, theory::EvidenceRecordPacket::Remove);
    }
  });
  connect(&_inventories, &InventoryRegistry::evidenceReplaced, this, &ClientGameObserver::synchronizeEvidence);
  connect(&_inventories, &InventoryRegistry::evidenceReset, this, [this](theory::InventoryId inventoryId) {
    for (const theory::EvidenceId evidenceId : _shippedEvidence.value(inventoryId))
    {
      shipEvidenceRecord(inventoryId, evidenceId, theory::EvidenceRecordPacket::Remove);
    }
    synchronizeAllEvidence();
  });

  connect(&_server, &Server::personalInventoriesToggled, this, [this] {
    for (const AOClient *client : _clients.clients())
    {
      shipInventoryUpdate(client->inventoryId);
    }
  });

  connect(&_viewer, &AOClient::sessionStatusChanged, this, [this](AOClient::SessionStatus status) {
    if (status == AOClient::SessionStatus::Active)
    {
      _shippedEvidence.clear();
      shipSnapshot();
    }
  });

  shipSnapshot();
}

bool kenji::ClientGameObserver::isEvidenceVisible(theory::InventoryId inventoryId, const theory::Evidence &evidence) const
{
  const theory::InventoryPermission permission = _inventories.hasPermission(inventoryId, _viewer.id);
  if (permission >= theory::InventoryPermission::Edit)
  {
    return true;
  }
  if (permission < theory::InventoryPermission::View || !isInventoryReachable(inventoryId))
  {
    return false;
  }
  // TODO MUST be removed when party system is implemented
  const bool personal = _server.getAreaById(_viewer.areaId())->inventoryId != inventoryId;
  return evidence.revealed || personal;
}

void kenji::ClientGameObserver::connectClient(AOClient *client)
{
  connect(client, &AOClient::nameChanged, this, [this, client] { shipPlayerUpdate(*client, theory::PlayerUpdatePacket::Name); });
  connect(client, &AOClient::characterChanged, this, [this, client] { shipPlayerUpdate(*client, theory::PlayerUpdatePacket::Character); });
  connect(client, &AOClient::characterNameChanged, this, [this, client] { shipPlayerUpdate(*client, theory::PlayerUpdatePacket::CharacterName); });
  connect(client, &AOClient::statusChanged, this, [this, client] { shipPlayerUpdate(*client, theory::PlayerUpdatePacket::Status); });
  connect(client, &AOClient::areaIdChanged, this, [this, client] {
    shipPlayerUpdate(*client, theory::PlayerUpdatePacket::AreaId);
    synchronizeAllEvidence();
  });
}

void kenji::ClientGameObserver::shipPlayerRecord(theory::PlayerId playerId, theory::PlayerRecordPacket::Action action)
{
  theory::PlayerRecordPacket packet;
  packet.playerId = playerId;
  packet.action = action;
  if (action == theory::PlayerRecordPacket::Add)
  {
    packet.inventoryId = _clients.client(playerId)->inventoryId;
  }
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipPlayerUpdate(const AOClient &client, theory::PlayerUpdatePacket::Property property)
{
  theory::PlayerUpdatePacket packet;
  packet.playerId = client.id;
  packet.property = property;

  switch (property)
  {
  default:
  case theory::PlayerUpdatePacket::NoProperty:
    break;
  case theory::PlayerUpdatePacket::Name:
    packet.data = theory::encodeJson(client.name());
    break;
  case theory::PlayerUpdatePacket::Character:
    packet.data = theory::encodeJson(client.character());
    break;
  case theory::PlayerUpdatePacket::CharacterName:
    packet.data = theory::encodeJson(client.characterName());
    break;
  case theory::PlayerUpdatePacket::AreaId:
    packet.data = theory::encodeJson(client.areaId());
    break;
  case theory::PlayerUpdatePacket::Status:
    packet.data = theory::encodeJson(client.status());
    break;
  }

  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipPlayerUpdates(const AOClient &client)
{
  shipPlayerUpdate(client, theory::PlayerUpdatePacket::Name);
  shipPlayerUpdate(client, theory::PlayerUpdatePacket::Character);
  shipPlayerUpdate(client, theory::PlayerUpdatePacket::CharacterName);
  shipPlayerUpdate(client, theory::PlayerUpdatePacket::AreaId);
  shipPlayerUpdate(client, theory::PlayerUpdatePacket::Status);
}

void kenji::ClientGameObserver::shipAreaRecord(const AreaData &area)
{
  theory::AreaRecordPacket packet;
  packet.action = theory::AreaRecordPacket::Add;
  packet.areaId = area.id;
  packet.inventoryId = area.inventoryId;
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipAreaUpdate(const AreaData &area, theory::AreaUpdatePacket::Property property)
{
  theory::AreaUpdatePacket packet;
  packet.areaId = area.id;
  packet.property = property;

  switch (property)
  {
  default:
  case theory::AreaUpdatePacket::NoProperty:
    break;
  case theory::AreaUpdatePacket::Name:
    packet.data = theory::encodeJson(area.name());
    break;
  case theory::AreaUpdatePacket::Status:
    packet.data = theory::encodeJson(area.status());
    break;
  case theory::AreaUpdatePacket::Ownership:
    packet.data = theory::encodeJson(area.owners());
    break;
  case theory::AreaUpdatePacket::Locked:
    packet.data = theory::encodeJson(area.lockStatus());
    break;
  }

  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipAreaUpdates(const AreaData &area)
{
  shipAreaUpdate(area, theory::AreaUpdatePacket::Name);
  shipAreaUpdate(area, theory::AreaUpdatePacket::Status);
  shipAreaUpdate(area, theory::AreaUpdatePacket::Ownership);
  shipAreaUpdate(area, theory::AreaUpdatePacket::Locked);
}

bool kenji::ClientGameObserver::isInventoryReachable(theory::InventoryId inventoryId) const
{
  const theory::AreaId areaId = _viewer.areaId();
  if (areaId == theory::NoAreaId)
  {
    return false;
  }
  if (_server.getAreaById(areaId)->inventoryId == inventoryId)
  {
    return true;
  }
  for (const AOClient *client : _clients.clientsInArea(areaId))
  {
    if (client->inventoryId == inventoryId)
    {
      return true;
    }
  }
  return false;
}

void kenji::ClientGameObserver::shipInventoryRecord(theory::InventoryId inventoryId, theory::InventoryRecordPacket::Action action)
{
  theory::InventoryRecordPacket packet;
  packet.action = action;
  packet.inventoryId = inventoryId;
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipInventoryUpdate(theory::InventoryId inventoryId)
{
  theory::InventoryUpdatePacket packet;
  packet.inventoryId = inventoryId;
  packet.property = theory::InventoryUpdatePacket::Permission;
  packet.data = theory::encodeJson(_inventories.hasPermission(inventoryId, _viewer.id));
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipEvidenceRecord(theory::InventoryId inventoryId, theory::EvidenceId evidenceId, theory::EvidenceRecordPacket::Action action)
{
  switch (action)
  {
  default:
  case theory::EvidenceRecordPacket::NoAction:
    break;
  case theory::EvidenceRecordPacket::Add:
    _shippedEvidence[inventoryId].insert(evidenceId);
    break;
  case theory::EvidenceRecordPacket::Remove:
    _shippedEvidence[inventoryId].remove(evidenceId);
    break;
  }

  theory::EvidenceRecordPacket packet;
  packet.action = action;
  packet.inventoryId = inventoryId;
  packet.evidenceId = evidenceId;
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::shipEvidenceUpdate(const theory::EvidenceItem &item)
{
  theory::EvidenceUpdatePacket packet;
  packet.evidenceId = item.id;
  packet.property = theory::EvidenceUpdatePacket::Snapshot;
  packet.data = theory::encodeJson(item.evidence);
  _viewer.shipPacket(packet);
}

void kenji::ClientGameObserver::synchronizeEvidence(theory::InventoryId inventoryId, const theory::EvidenceItem &item)
{
  const bool visible = isEvidenceVisible(inventoryId, item.evidence);
  const bool shipped = _shippedEvidence.value(inventoryId).contains(item.id);
  if (visible && shipped)
  {
    shipEvidenceUpdate(item);
  }
  else if (visible)
  {
    shipEvidenceRecord(inventoryId, item.id, theory::EvidenceRecordPacket::Add);
    shipEvidenceUpdate(item);
  }
  else if (shipped)
  {
    shipEvidenceRecord(inventoryId, item.id, theory::EvidenceRecordPacket::Remove);
  }
}

void kenji::ClientGameObserver::synchronizeAllEvidence()
{
  for (const theory::InventoryId inventoryId : _inventories.inventories())
  {
    for (const theory::EvidenceItem &item : _inventories.inventory(inventoryId))
    {
      const bool visible = isEvidenceVisible(inventoryId, item.evidence);
      const bool shipped = _shippedEvidence.value(inventoryId).contains(item.id);
      if (visible && !shipped)
      {
        shipEvidenceRecord(inventoryId, item.id, theory::EvidenceRecordPacket::Add);
        shipEvidenceUpdate(item);
      }
      else if (!visible && shipped)
      {
        shipEvidenceRecord(inventoryId, item.id, theory::EvidenceRecordPacket::Remove);
      }
    }
  }
}

void kenji::ClientGameObserver::shipSnapshot()
{
  for (const AOClient *client : _clients.clients())
  {
    shipPlayerRecord(client->id, theory::PlayerRecordPacket::Add);
    shipPlayerUpdates(*client);
  }
  for (const AreaData *area : _server.getAreas())
  {
    shipAreaRecord(*area);
    shipAreaUpdates(*area);
  }
  for (const theory::InventoryId inventoryId : _inventories.inventories())
  {
    shipInventoryRecord(inventoryId, theory::InventoryRecordPacket::Add);
    shipInventoryUpdate(inventoryId);
  }
  synchronizeAllEvidence();
}
