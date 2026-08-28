#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "music_manager.h"
#include "server.h"

// This file is for commands under the music category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdPlay(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  QString l_song = argv.join(" ");
  if (m_is_dj_blocked)
  {
    sendServerMessage("You are blocked from changing the music.");
    return;
  }
  if (l_song == "sin.mp3")
  {
    drop();
    return;
  }
  if ((l_song.startsWith("http://", Qt::CaseInsensitive) || l_song.startsWith("https://", Qt::CaseInsensitive)) && !m_music_manager->validateSong(l_song, ConfigManager::cdnList()))
  {
    sendServerMessage("The song you tried to play is not from an approved CDN.");
    return;
  }
  AreaData *l_area = server->getAreaById(areaId());
  const ACLRole l_role = server->getACLRolesHandler()->getRoleById(m_acl_role_id);
  if (!l_area->owners().contains(clientId()) && !l_area->isPlayEnabled() && !l_role.checkPermission(ACLRole::CM))
  { // Make sure we have permission to play music
    sendServerMessage("Free music play is disabled in this area.");
    return;
  }
  std::optional<QString> l_final_track;
  if (!l_song.trimmed().isEmpty())
  {
    l_final_track = l_song;
  }

  l_area->setCurrentMusic(l_final_track);
  theory::MusicChangedPacket l_music_change;
  l_music_change.track = l_final_track;
  l_music_change.character = m_character;
  l_music_change.characterName = characterName();
  l_music_change.channel = theory::MusicChannel::Music;
  l_music_change.loop = true;
  server->broadcastToArea(l_music_change, areaId());
}

void kenji::AOClient::cmdPlayAmbience(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  if (m_is_dj_blocked)
  {
    sendServerMessage("You are blocked from changing the ambience.");
    return;
  }
  AreaData *l_area = server->getAreaById(areaId());
  if (!l_area->owners().contains(clientId()) && !l_area->isPlayEnabled())
  { // Make sure we have permission to play music
    sendServerMessage("Free ambience play is disabled in this area.");
    return;
  }
  QString l_song = argv.join(" ");
  if ((l_song.startsWith("http://", Qt::CaseInsensitive) || l_song.startsWith("https://", Qt::CaseInsensitive)) && !m_music_manager->validateSong(l_song, ConfigManager::cdnList()))
  {
    sendServerMessage("The song you tried to play is not from an approved CDN.");
    return;
  }
  std::optional<QString> l_final_track;
  if (!l_song.trimmed().isEmpty())
  {
    l_final_track = l_song;
  }

  l_area->setAmbience(l_final_track);
  theory::MusicChangedPacket l_music_change;
  l_music_change.track = l_final_track;
  l_music_change.character = theory::NoCharacterId;
  l_music_change.characterName = characterName();
  l_music_change.channel = theory::MusicChannel::Ambient;
  l_music_change.loop = true;
  server->broadcastToArea(l_music_change, areaId());
}

void kenji::AOClient::cmdCurrentMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->currentMusic())
  {
    sendServerMessage("The current song is " + l_area->currentMusic().value());
  }
  else
  {
    sendServerMessage("There is no music playing.");
  }
}

void kenji::AOClient::cmdBlockDj(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  int l_uid = argv[0].toInt(&conv_ok);
  if (!conv_ok)
  {
    sendServerMessage("Invalid user ID.");
    return;
  }

  AOClient *l_target = server->getClientByID(l_uid);

  if (l_target == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }

  if (l_target->m_is_dj_blocked)
  {
    sendServerMessage("That player is already DJ blocked!");
  }
  else
  {
    sendServerMessage("DJ blocked player.");
    l_target->sendServerMessage("You were blocked from changing the music by a moderator. " + getReprimand());
  }
  l_target->m_is_dj_blocked = true;
}

void kenji::AOClient::cmdUnBlockDj(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  int l_uid = argv[0].toInt(&conv_ok);
  if (!conv_ok)
  {
    sendServerMessage("Invalid user ID.");
    return;
  }

  AOClient *l_target = server->getClientByID(l_uid);

  if (l_target == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }

  if (!l_target->m_is_dj_blocked)
  {
    sendServerMessage("That player is not DJ blocked!");
  }
  else
  {
    sendServerMessage("DJ permissions restored to player.");
    l_target->sendServerMessage("A moderator restored your music permissions. " + getReprimand(true));
  }
  l_target->m_is_dj_blocked = false;
}

void kenji::AOClient::cmdToggleMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleMusic();
  QString l_state = l_area->isMusicAllowed() ? "allowed." : "disallowed.";
  sendServerMessage("Music in this area is now " + l_state);
}

void kenji::AOClient::cmdToggleJukebox(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleJukebox();
  QString l_state = l_area->isjukeboxEnabled() ? "enabled." : "disabled.";
  sendServerMessageArea("The jukebox in this area has been " + l_state);
}

void kenji::AOClient::cmdAddMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  // This needs some explanation.
  // Kenji has no concept of argument count,so any space is interpreted as a new element
  // in the QStringList. This works fine until someone enters something with a space.
  // Since we can't preencode those elements, we join all as a string and use a delimiter
  // that does not exist in file and URL paths. I decided on the ol' reliable ','.
  QString l_argv_string = argv.join(" ");
  QStringList l_argv = l_argv_string.split(",");

  bool l_success = false;
  if (l_argv.size() == 1)
  {
    QString l_song_name = l_argv.value(0);
    l_success = m_music_manager->addCustomSong(l_song_name, l_song_name, 0, areaId());
  }

  if (l_argv.size() == 2)
  {
    QString l_song_name = l_argv.value(0);
    QString l_true_name = l_argv.value(1);
    l_success = m_music_manager->addCustomSong(l_song_name, l_true_name, 0, areaId());
  }

  if (l_argv.size() == 3)
  {
    QString l_song_name = l_argv.value(0);
    QString l_true_name = l_argv.value(1);
    bool ok;
    int l_song_duration = l_argv.value(2).toInt(&ok);
    if (!ok)
    {
      l_song_duration = 0;
    }
    l_success = m_music_manager->addCustomSong(l_song_name, l_true_name, l_song_duration, areaId());
  }

  if (l_argv.size() >= 4)
  {
    sendServerMessage("Too many arguments. Addition of song has failed.");
    return;
  }

  QString l_message = l_success ? "succeeded." : "failed.";
  sendServerMessage("The addition of the song has " + l_message);
}

void kenji::AOClient::cmdAddMusicCategory(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  bool l_success = m_music_manager->addCustomCategory(argv.join(" "), areaId());
  QString l_message = l_success ? "succeeded." : "failed.";
  sendServerMessage("The addition of the category has " + l_message);
}

void kenji::AOClient::cmdRemoveCustomMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  bool l_success = m_music_manager->removeCustomMusic(argv.join(" "), areaId());
  QString l_message = l_success ? "succeeded." : "failed.";
  sendServerMessage("The removal of the entry has " + l_message);
}

void kenji::AOClient::cmdToggleCustomMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);
  bool l_status = m_music_manager->toggleCustomMusicEnabled(areaId());
  QString l_message = (l_status) ? "enabled." : "disabled.";
  sendServerMessage("Global musiclist has been " + l_message);
}

void kenji::AOClient::cmdClearCustomMusic(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);
  m_music_manager->clearCustomMusicList(areaId());
  sendServerMessage("Custom songs have been cleared.");
}

void kenji::AOClient::cmdJukeboxSkip(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QString l_name = characterName().value_or(m_character.toString());

  AreaData *l_area = server->getAreaById(areaId());

  if (l_area->isjukeboxEnabled())
  {
    if (l_area->getJukeboxQueueSize() >= 1)
    {
      l_area->switchJukeboxSong();
      sendServerMessageArea(l_name + " has forced a skip. Playing the next available song.");
      return;
    }
    sendServerMessage("Unable to skip song. Jukebox is currently empty.");
    return;
  }
  sendServerMessage("Unable to skip song. The jukebox is not running.");
}
