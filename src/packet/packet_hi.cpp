#include "ao_client.h"

#include "area_data.h"
#include "server.h"

void kenji::AOClient::shipSnapshot()
{
  while (!_queuedPackets.isEmpty())
  {
    shipPacket(*_queuedPackets.dequeue());
  }

  AreaData *l_area = server->getAreaById(areaId());

  theory::ServerSettingsPacket l_settings;
  l_settings.settings = server->serverSettings();
  shipPacket(l_settings);

  sendCharacterList();

  theory::MusicListPacket l_music_list;
  l_music_list.playlists = m_music_manager->playlists(areaId());
  shipPacket(l_music_list);

  theory::PenaltyPacket l_def_penalty;
  l_def_penalty.bar = theory::HealthBar::Defense;
  l_def_penalty.value = l_area->defHP();
  shipPacket(l_def_penalty);

  theory::PenaltyPacket l_pro_penalty;
  l_pro_penalty.bar = theory::HealthBar::Prosecution;
  l_pro_penalty.value = l_area->proHP();
  shipPacket(l_pro_penalty);

  theory::BackgroundPacket l_background;
  l_background.background = l_area->background();
  l_background.side = l_area->side();
  l_background.display = true;
  shipPacket(l_background);

  server->shipGlobalTimer(id);
  l_area->shipTimers(id);
}

void kenji::AOClient::beginSession()
{
  if (server->getAreaById(areaId()) == nullptr)
  {
    setAreaId(server->defaultArea()->id);
  }

  shipSnapshot();

  theory::WelcomePacket l_welcome;
  l_welcome.playerId = id;
  shipPacket(l_welcome);

  server->getAreaById(areaId())->addClient(theory::NoCharacterId, id);
}

void kenji::AOClient::resumeSession()
{
  shipSnapshot();

  theory::WelcomePacket l_welcome;
  l_welcome.playerId = id;
  shipPacket(l_welcome);

  sendCharacterSelection();
}

void kenji::AOClient::sendCharacterList()
{
  theory::CharacterListPacket l_character_list;

  if (m_is_charcursed)
  {
    l_character_list.characters = m_charcurse_list;
  }
  else
  {
    l_character_list.characters = server->getCharacters();
  }

  shipPacket(l_character_list);
}

void kenji::AOClient::sendCharacterSelection()
{
  theory::CharacterAcceptedPacket l_accepted;
  l_accepted.character = m_character;
  shipPacket(l_accepted);
}
