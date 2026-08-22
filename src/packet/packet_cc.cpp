#include "ao_client.h"

#include "server.h"

void kenji::AOClient::process(const theory::ChangeCharacterPacket &packet)
{
  theory::CharacterId l_selected_char_id = packet.characterId;

  if (m_is_charcursed && l_selected_char_id != theory::NoCharacterId)
  {
    if (l_selected_char_id < 0 || l_selected_char_id >= m_charcurse_list.size())
    {
      drop(theory::ErrorPacket::ProtocolError, "Packet : change_character\nCharacter ID out of range.");
      return;
    }

    l_selected_char_id = m_charcurse_list.at(l_selected_char_id);
  }

  if (l_selected_char_id != theory::NoCharacterId && (l_selected_char_id < 0 || l_selected_char_id >= server->getCharacterCount()))
  {
    drop(theory::ErrorPacket::ProtocolError, "Packet : change_character\nCharacter ID out of range.");
    return;
  }

  if (changeCharacter(l_selected_char_id))
  {
    m_char_id = l_selected_char_id;
  }
  else
  {
    sendServerMessage("That character is unavailable.");
  }
}
