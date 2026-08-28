#include "player_state_observer.h"

#include "core/json_codec.h"

kenji::PlayerStateObserver::PlayerStateObserver(QObject *parent)
    : QObject{parent}
{}

kenji::PlayerStateObserver::~PlayerStateObserver()
{}

void kenji::PlayerStateObserver::registerClient(AOClient *client)
{
  Q_ASSERT(!m_client_list.contains(client));

  theory::PlayerRosterPacket l_roster;
  l_roster.playerId = client->playerId();
  l_roster.action = theory::PlayerRosterPacket::Add;
  broadcast(l_roster);

  m_client_list.append(client);

  auto notify = [this, client](theory::PlayerUpdatePacket::Property property) {
    broadcast(playerUpdate(*client, property));
  };

  connect(client, &AOClient::nameChanged, this, [notify] { notify(theory::PlayerUpdatePacket::Name); });
  connect(client, &AOClient::characterChanged, this, [notify] { notify(theory::PlayerUpdatePacket::Character); });
  connect(client, &AOClient::characterNameChanged, this, [notify] { notify(theory::PlayerUpdatePacket::CharacterName); });
  connect(client, &AOClient::areaIdChanged, this, [notify] { notify(theory::PlayerUpdatePacket::AreaId); });
  connect(client, &AOClient::statusChanged, this, [notify] { notify(theory::PlayerUpdatePacket::Status); });
  connect(client, &AOClient::sessionStatusChanged, this, [this, client](AOClient::SessionStatus status) {
    if (status == AOClient::SessionStatus::Active)
    {
      shipRoster(client);
    }
  });

  shipRoster(client);
}

void kenji::PlayerStateObserver::unregisterClient(AOClient *client)
{
  Q_ASSERT(m_client_list.contains(client));

  client->disconnect(this);

  m_client_list.removeAll(client);

  theory::PlayerRosterPacket l_roster;
  l_roster.playerId = client->playerId();
  l_roster.action = theory::PlayerRosterPacket::Remove;
  broadcast(l_roster);
}

theory::PlayerUpdatePacket kenji::PlayerStateObserver::playerUpdate(const AOClient &client, theory::PlayerUpdatePacket::Property property)
{
  theory::PlayerUpdatePacket packet;
  packet.playerId = client.playerId();
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

  return packet;
}

void kenji::PlayerStateObserver::broadcast(const theory::Packet &packet)
{
  for (AOClient *client : qAsConst(m_client_list))
  {
    client->shipPacket(packet);
  }
}

void kenji::PlayerStateObserver::shipRoster(AOClient *target)
{
  for (AOClient *i_client : qAsConst(m_client_list))
  {
    theory::PlayerRosterPacket l_entry;
    l_entry.playerId = i_client->playerId();
    l_entry.action = theory::PlayerRosterPacket::Add;
    target->shipPacket(l_entry);

    target->shipPacket(playerUpdate(*i_client, theory::PlayerUpdatePacket::Name));
    target->shipPacket(playerUpdate(*i_client, theory::PlayerUpdatePacket::Character));
    target->shipPacket(playerUpdate(*i_client, theory::PlayerUpdatePacket::CharacterName));
    target->shipPacket(playerUpdate(*i_client, theory::PlayerUpdatePacket::AreaId));
    target->shipPacket(playerUpdate(*i_client, theory::PlayerUpdatePacket::Status));
  }
}
