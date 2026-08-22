#include "aoclient.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

void kenji::AOClient::shipSnapshot()
{
  while (!_queuedPackets.isEmpty())
  {
    shipPacket(*_queuedPackets.dequeue());
  }

  AreaData *l_area = server->getAreaById(areaId());

  sendCharacterList();

  theory::MusicListPacket l_music_list;
  l_music_list.playlists = m_music_manager->playlists(areaId());
  shipPacket(l_music_list);

  theory::AreaListPacket l_area_list;
  l_area_list.areas = server->getAreaNames();
  shipPacket(l_area_list);

  sendEvidenceList(l_area);

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

  sendServerMessage("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");

  fullArup(); // Give client all the area data

  server->shipGlobalTimer(clientId());
  l_area->shipTimers(clientId());
}

void kenji::AOClient::beginSession()
{
  shipSnapshot();

  theory::WelcomePacket l_welcome;
  l_welcome.clientId = clientId();
  shipPacket(l_welcome);

  server->getAreaById(areaId())->addClient(-1, clientId());
}

void kenji::AOClient::resumeSession()
{
  shipSnapshot();

  theory::WelcomePacket l_welcome;
  l_welcome.clientId = clientId();
  shipPacket(l_welcome);

  sendCharacterSelection();
}

void kenji::AOClient::sendCharacterList()
{
  theory::CharacterListPacket l_character_list;

  if (m_is_charcursed)
  {
    for (theory::CharacterId l_char_id : qAsConst(m_charcurse_list))
    {
      l_character_list.characters.append(server->getCharacterById(l_char_id));
    }
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
  l_accepted.characterId = m_is_charcursed ? m_charcurse_list.indexOf(m_char_id) : m_char_id;
  shipPacket(l_accepted);
}
