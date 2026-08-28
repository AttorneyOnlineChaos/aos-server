#include "ao_client.h"

#include "area_data.h"
#include "music_manager.h"
#include "server.h"

void kenji::AOClient::process(const theory::PlayMusicPacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  std::optional<QString> l_final_track;
  if (packet.track && !packet.track->trimmed().isEmpty())
  {
    l_final_track = packet.track.value();
  }

  if (l_final_track && l_final_track->contains(".."))
  {
    sendServerMessage("Invalid music track.");
    return;
  }

  if (l_final_track && !m_music_manager->findTrack(l_final_track.value(), areaId()))
  {
    return;
  }

  if (isSpectator())
  {
    sendServerMessage("Spectators are blocked from changing the music.");
    return;
  }

  if (l_area->lockStatus() == theory::AreaLockStatus::Spectatable && !l_area->invited().contains(clientId()) && !checkPermission(ACLRole::BYPASS_LOCKS))
  {
    sendServerMessage("Spectators are blocked from changing the music.");
    return;
  }

  if (m_is_dj_blocked)
  {
    sendServerMessage("You are blocked from changing the music.");
    return;
  }
  if (!l_area->isMusicAllowed() && !checkPermission(ACLRole::CM))
  {
    sendServerMessage("Music is disabled in this area.");
    return;
  }

  // Jukebox intercepts the direct playing of messages.
  if (l_area->isjukeboxEnabled())
  {
    if (!l_final_track)
    {
      sendServerMessage("The jukebox is enabled in this area.");
      return;
    }

    QString l_jukebox_reply = l_area->addJukeboxSong(l_final_track.value());
    sendServerMessage(l_jukebox_reply);
    return;
  }

  theory::MusicChangedPacket l_music_change;
  l_music_change.track = l_final_track;
  l_music_change.character = m_character;
  l_music_change.characterName = characterName();
  l_music_change.channel = theory::MusicChannel::Music;
  l_music_change.loop = !packet.noRepeat;
  l_music_change.effects = packet.effects;
  server->broadcastToArea(l_music_change, areaId());

  m_logger.logMusic((m_character.toString() + " " + characterName().value_or(QString())), name(), m_ipid, l_area->name(), l_final_track.value_or(QString()));

  l_area->setCurrentMusic(l_final_track);
}

void kenji::AOClient::process(const theory::ChangeAreaPacket &packet)
{
  if (packet.areaId < 0 || packet.areaId >= server->getAreaCount())
  {
    return;
  }

  changeArea(packet.areaId);
}
