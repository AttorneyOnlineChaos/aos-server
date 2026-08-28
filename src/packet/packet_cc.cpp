#include "ao_client.h"

void kenji::AOClient::process(const theory::ChangeCharacterPacket &packet)
{
  if (!changeCharacter(packet.character))
  {
    sendServerMessage("That character is unavailable.");
  }
}
