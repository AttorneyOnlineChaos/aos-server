#include "area_data.h"

#include "config_manager.h"
#include "core/json_codec.h"
#include "music_manager.h"
#include "protocol/packets/music_packets.h"

#include <QRegularExpression>

#include <algorithm>

kenji::AreaData::AreaData(const QString &p_name, int p_index, MusicManager *p_music_manager, Broadcaster &p_broadcaster)
    : m_index(p_index)
    , m_music_manager(p_music_manager)
    , m_broadcaster{p_broadcaster}
    , m_playerCount(0)
    , m_status(theory::AreaStatus::Idle)
    , m_locked(theory::AreaLockStatus::Unlocked)
    , m_document("No document.")
    , m_area_message("No area message set.")
    , m_defHP(10)
    , m_proHP(10)
    , m_currentMusic()
    , m_statement(0)
    , m_judgelog()
    , m_lastICMessage()
    , m_send_area_message(false)
    , m_can_send_wtce(true)
    , m_can_use_shouts(true)
{
  QStringList name_split = p_name.split(":");
  name_split.removeFirst();
  m_name = name_split.join(":");
  if (m_name.isEmpty())
  {
    m_name = "Unnamed Area";
  }
  m_display_name = "[" + QString::number(m_index) + "] " + m_name;
  QSettings *areas_ini = ConfigManager::areaData();
  areas_ini->beginGroup(p_name);
  m_background = areas_ini->value("background", "gs4").toString();
  m_isProtected = areas_ini->value("protected_area", "false").toBool();
  m_iniswapAllowed = areas_ini->value("iniswap_allowed", "true").toBool();
  m_bgLocked = areas_ini->value("bg_locked", "false").toBool();
  m_eviMod = QVariant(areas_ini->value("evidence_mod", "FFA").toString().toUpper()).value<EvidenceMod>();
  m_blankpostingAllowed = areas_ini->value("blankposting_allowed", "true").toBool();
  m_area_message = areas_ini->value("area_message").toString();
  m_send_area_message = areas_ini->value("send_area_message_on_join", false).toBool();
  m_forceImmediate = areas_ini->value("force_immediate", "false").toBool();
  m_toggleMusic = areas_ini->value("toggle_music", "true").toBool();
  m_shownameAllowed = areas_ini->value("shownames_allowed", "true").toBool();
  m_ignoreBgList = areas_ini->value("ignore_bglist", "false").toBool();
  m_jukebox = areas_ini->value("jukebox_enabled", "false").toBool();
  m_playcmd = areas_ini->value("playcmd_enabled", "false").toBool();
  m_can_send_wtce = areas_ini->value("wtce_enabled", "true").toBool();
  m_can_use_shouts = areas_ini->value("shouts_enabled", "true").toBool();
  areas_ini->endGroup();
  for (theory::TimerId i = 1; i < theory::TimerCount; i++)
  {
    Timer *l_timer = new Timer(i, this);
    m_timers.insert(i, l_timer);

    connect(l_timer, &Timer::stateChanged, this, [this, l_timer] {
      m_broadcaster.broadcastToArea(makeTimerPacket(*l_timer, theory::TimerPacket::State), m_index);
      m_broadcaster.broadcastToArea(makeTimerPacket(*l_timer, theory::TimerPacket::Tick), m_index);
    });

    connect(l_timer, &Timer::visibilityChanged, this, [this, l_timer] { m_broadcaster.broadcastToArea(makeTimerPacket(*l_timer, theory::TimerPacket::Visibility), m_index); });
  }
  m_jukebox_timer = new QTimer();
  connect(m_jukebox_timer, &QTimer::timeout, this, &AreaData::switchJukeboxSong);
  m_message_floodguard_timer = new QTimer(this);
  connect(m_message_floodguard_timer, &QTimer::timeout, this, &AreaData::allowMessage);
}

const QMap<QString, theory::AreaStatus> kenji::AreaData::map_statuses = {
    {"IDLE", theory::AreaStatus::Idle},
    {"ROLEPLAY", theory::AreaStatus::Roleplay},
    {"CASING", theory::AreaStatus::Casing},
    {"LOOKING-FOR-PLAYERS", theory::AreaStatus::LookingForPlayers},
    {"RECESS", theory::AreaStatus::Recess},
    {"GAMING", theory::AreaStatus::Gaming},
    {"BUILDING", theory::AreaStatus::Building},
    {"STARTING", theory::AreaStatus::Starting},
};

void kenji::AreaData::removeClient(theory::CharacterId f_charId, int f_userId)
{
  --m_playerCount;

  if (f_charId != theory::NoCharacterId)
  {
    m_charactersTaken.removeAll(f_charId);
  }
  m_joined_ids.removeAll(f_userId);
}

void kenji::AreaData::addClient(theory::CharacterId f_charId, int f_userId)
{
  ++m_playerCount;

  if (f_charId != theory::NoCharacterId)
  {
    m_charactersTaken.append(f_charId);
  }
  m_joined_ids.append(f_userId);
  Q_EMIT userJoinedArea(m_index, f_userId);
  // Send out ambience as well.
  theory::MusicChangedPacket l_ambience;
  l_ambience.track = m_currentAmbience;
  l_ambience.channel = theory::MusicChannel::Ambient;
  l_ambience.loop = true;
  m_broadcaster.broadcastToPlayer(l_ambience, f_userId);
  // We auto-loop this so you'll never sit in silence unless wanted.
  theory::MusicChangedPacket l_music;
  l_music.track = m_currentMusic;
  l_music.channel = theory::MusicChannel::Music;
  l_music.loop = true;
  m_broadcaster.broadcastToPlayer(l_music, f_userId);
}

QList<int> kenji::AreaData::owners() const
{
  return m_owners;
}

void kenji::AreaData::addOwner(int f_clientId)
{
  m_owners.append(f_clientId);
  m_invited.append(f_clientId);
}

bool kenji::AreaData::removeOwner(int f_clientId)
{
  m_owners.removeAll(f_clientId);
  m_invited.removeAll(f_clientId);

  if (m_owners.isEmpty() && m_locked != theory::AreaLockStatus::Unlocked)
  {
    m_locked = theory::AreaLockStatus::Unlocked;
    return true;
  }

  return false;
}

bool kenji::AreaData::blankpostingAllowed() const
{
  return m_blankpostingAllowed;
}

void kenji::AreaData::toggleBlankposting()
{
  m_blankpostingAllowed = !m_blankpostingAllowed;
}

bool kenji::AreaData::isProtected() const
{
  return m_isProtected;
}

theory::AreaLockStatus kenji::AreaData::lockStatus() const
{
  return m_locked;
}

bool kenji::AreaData::isjukeboxEnabled() const
{
  return m_jukebox;
}

int kenji::AreaData::getJukeboxQueueSize() const
{
  return m_jukebox_queue.size();
}

bool kenji::AreaData::isPlayEnabled() const
{
  return m_playcmd;
}

void kenji::AreaData::lock()
{
  m_locked = theory::AreaLockStatus::Locked;
}

void kenji::AreaData::unlock()
{
  m_locked = theory::AreaLockStatus::Unlocked;
}

void kenji::AreaData::spectatable()
{
  m_locked = theory::AreaLockStatus::Spectatable;
}

bool kenji::AreaData::invite(int f_clientId)
{
  if (m_invited.contains(f_clientId))
  {
    return false;
  }

  m_invited.append(f_clientId);
  return true;
}

bool kenji::AreaData::uninvite(int f_clientId)
{
  if (!m_invited.contains(f_clientId))
  {
    return false;
  }

  m_invited.removeAll(f_clientId);
  return true;
}

int kenji::AreaData::playerCount() const
{
  return m_playerCount;
}

QList<kenji::Timer *> kenji::AreaData::timers() const
{
  return m_timers.values();
}

kenji::Timer *kenji::AreaData::timer(theory::TimerId f_timer_id) const
{
  return m_timers.value(f_timer_id, nullptr);
}

void kenji::AreaData::shipTimers(theory::ClientId f_player_id)
{
  for (Timer *l_timer : m_timers)
  {
    m_broadcaster.broadcastToPlayer(makeTimerPacket(*l_timer, theory::TimerPacket::Visibility), f_player_id);
    m_broadcaster.broadcastToPlayer(makeTimerPacket(*l_timer, theory::TimerPacket::State), f_player_id);
    m_broadcaster.broadcastToPlayer(makeTimerPacket(*l_timer, theory::TimerPacket::Tick), f_player_id);
  }
}

void kenji::AreaData::synchronize()
{
  for (Timer *l_timer : m_timers)
  {
    if (l_timer->state() != theory::TimerState::Running)
    {
      continue;
    }
    m_broadcaster.broadcastToArea(makeTimerPacket(*l_timer, theory::TimerPacket::Tick), m_index);
  }
}

QString kenji::AreaData::name() const
{
  return m_name;
}

int kenji::AreaData::index() const
{
  return m_index;
}

QString kenji::AreaData::displayName() const
{
  return m_display_name;
}

QList<theory::CharacterId> kenji::AreaData::charactersTaken() const
{
  return m_charactersTaken;
}

bool kenji::AreaData::changeCharacter(theory::CharacterId f_from, theory::CharacterId f_to)
{
  if (m_charactersTaken.contains(f_to))
  {
    return false;
  }

  if (f_to != theory::NoCharacterId)
  {
    if (f_from != theory::NoCharacterId)
    {
      m_charactersTaken.removeAll(f_from);
    }
    m_charactersTaken.append(f_to);
    return true;
  }

  if (f_to == theory::NoCharacterId && f_from != theory::NoCharacterId)
  {
    m_charactersTaken.removeAll(f_from);
  }

  return false;
}

QList<kenji::AreaData::Evidence> kenji::AreaData::evidence() const
{
  return m_evidence;
}

void kenji::AreaData::swapEvidence(int f_eviId1, int f_eviId2)
{
  m_evidence.swapItemsAt(f_eviId1, f_eviId2);
}

void kenji::AreaData::appendEvidence(const AreaData::Evidence &f_evi_r)
{
  m_evidence.append(f_evi_r);
}

void kenji::AreaData::deleteEvidence(int f_eviId)
{
  m_evidence.removeAt(f_eviId);
}

void kenji::AreaData::replaceEvidence(int f_eviId, const AreaData::Evidence &f_newEvi_r)
{
  m_evidence.replace(f_eviId, f_newEvi_r);
}

void kenji::AreaData::setEvidenceOwnerToAll(int f_eviId)
{
  if (f_eviId < 0 || f_eviId >= m_evidence.size())
  {
    return;
  }

  Evidence &evidence = m_evidence[f_eviId];
  QString description = evidence.description;

  // Search for owner tag in description
  static const QRegularExpression ownerRegex("<owner=(.*?)>");
  QRegularExpressionMatch match = ownerRegex.match(description);

  if (match.hasMatch())
  {
    // Replace existing owner tag with <owner=all>
    description.replace(ownerRegex, "<owner=all>");
  }
  else
  {
    // If no owner tag exists, add <owner=all> at the beginning
    description = "<owner=all>\n" + description;
  }

  evidence.description = description;
}

theory::AreaStatus kenji::AreaData::status() const
{
  return m_status;
}

void kenji::AreaData::changeStatus(theory::AreaStatus status)
{
  m_status = status;
}

QList<int> kenji::AreaData::invited() const
{
  return m_invited;
}

bool kenji::AreaData::isMusicAllowed() const
{
  return m_toggleMusic;
}

bool kenji::AreaData::isMessageAllowed() const
{
  return m_can_send_ic_messages;
}

bool kenji::AreaData::isWtceAllowed() const
{
  return m_can_send_wtce;
}

bool kenji::AreaData::isShoutAllowed() const
{
  return m_can_use_shouts;
}

bool kenji::AreaData::isMedievalMode() const
{
  return m_medieval_mode;
}

void kenji::AreaData::startMessageFloodguard(int f_duration)
{
  m_can_send_ic_messages = false;
  m_message_floodguard_timer->setSingleShot(true);
  m_message_floodguard_timer->start(f_duration);
}

void kenji::AreaData::toggleMusic()
{
  m_toggleMusic = !m_toggleMusic;
}

void kenji::AreaData::setEviMod(const EvidenceMod &f_eviMod_r)
{
  m_eviMod = f_eviMod_r;
}

void kenji::AreaData::setTestimonyRecording(const TestimonyRecording &f_testimonyRecording_r)
{
  m_testimonyRecording = f_testimonyRecording_r;
}

void kenji::AreaData::restartTestimony()
{
  m_testimonyRecording = TestimonyRecording::PLAYBACK;
  m_statement = 0;
}

void kenji::AreaData::clearTestimony()
{
  m_testimonyRecording = AreaData::TestimonyRecording::STOPPED;
  m_statement = -1;
  m_testimony.clear();
}

bool kenji::AreaData::forceImmediate() const
{
  return m_forceImmediate;
}

void kenji::AreaData::toggleImmediate()
{
  m_forceImmediate = !m_forceImmediate;
}

const theory::IcMessagePacket &kenji::AreaData::lastICMessage() const
{
  return m_lastICMessage;
}

void kenji::AreaData::updateLastICMessage(const theory::IcMessagePacket &f_lastMessage_r)
{
  m_lastICMessage = f_lastMessage_r;
}

QStringList kenji::AreaData::judgelog() const
{
  return m_judgelog;
}

void kenji::AreaData::appendJudgelog(const QString &f_newLog_r)
{
  if (m_judgelog.size() == 10)
  {
    m_judgelog.removeFirst();
  }

  m_judgelog.append(f_newLog_r);
}

int kenji::AreaData::statement() const
{
  return m_statement;
}

void kenji::AreaData::recordStatement(const theory::IcMessagePacket &f_newStatement_r)
{
  ++m_statement;
  m_testimony.append(f_newStatement_r);
}

void kenji::AreaData::addStatement(int f_position, const theory::IcMessagePacket &f_newStatement_r)
{
  m_testimony.insert(f_position, f_newStatement_r);
}

void kenji::AreaData::replaceStatement(int f_position, const theory::IcMessagePacket &f_newStatement_r)
{
  m_testimony.replace(f_position, f_newStatement_r);
}

void kenji::AreaData::removeStatement(int f_position)
{
  m_testimony.remove(f_position);
  --m_statement;
}

QPair<theory::IcMessagePacket, kenji::AreaData::TestimonyProgress> kenji::AreaData::jumpToStatement(int f_position)
{
  if (m_testimony.size() <= 1)
  {
    return {theory::IcMessagePacket{}, TestimonyProgress::STAYED_AT_FIRST};
  }

  m_statement = f_position;

  if (m_statement > m_testimony.size() - 1)
  {
    m_statement = 1;
    return {m_testimony.at(m_statement), TestimonyProgress::LOOPED};
  }
  if (m_statement <= 1)
  {
    m_statement = 1;
    return {m_testimony.at(m_statement), TestimonyProgress::STAYED_AT_FIRST};
  }
  else
  {
    return {m_testimony.at(m_statement), TestimonyProgress::OK};
  }
}

const QList<theory::IcMessagePacket> &kenji::AreaData::testimony() const
{
  return m_testimony;
}

kenji::AreaData::TestimonyRecording kenji::AreaData::testimonyRecording() const
{
  return m_testimonyRecording;
}

kenji::AreaData::EvidenceMod kenji::AreaData::eviMod() const
{
  return m_eviMod;
}

bool kenji::AreaData::addNotecard(const QString &f_owner_r, const QString &f_notecard_r)
{
  m_notecards[f_owner_r] = f_notecard_r;

  if (f_notecard_r.isNull())
  {
    m_notecards.remove(f_owner_r);
    return false;
  }

  return true;
}

QStringList kenji::AreaData::getNotecards()
{
  QMapIterator<QString, QString> l_noteIter(m_notecards);
  QStringList l_notecards;

  while (l_noteIter.hasNext())
  {
    l_noteIter.next();
    l_notecards << l_noteIter.key() << ": " << l_noteIter.value() << "\n";
  }

  m_notecards.clear();

  return l_notecards;
}

void kenji::AreaData::setAmbience(const std::optional<QString> &f_newSong_r)
{
  m_currentAmbience = f_newSong_r;
}

std::optional<QString> kenji::AreaData::currentMusic() const
{
  return m_currentMusic;
}

void kenji::AreaData::setCurrentMusic(const std::optional<QString> &f_current_song)
{
  m_currentMusic = f_current_song;
}

int kenji::AreaData::proHP() const
{
  return m_proHP;
}

void kenji::AreaData::changeHP(AreaData::Side f_side, int f_newHP)
{
  if (f_side == Side::DEFENCE)
  {
    m_defHP = std::min(std::max(0, f_newHP), 10);
  }
  else if (f_side == Side::PROSECUTOR)
  {
    m_proHP = std::min(std::max(0, f_newHP), 10);
  }
}

int kenji::AreaData::defHP() const
{
  return m_defHP;
}

QString kenji::AreaData::document() const
{
  return m_document;
}

void kenji::AreaData::changeDoc(const QString &f_newDoc_r)
{
  m_document = f_newDoc_r;
}

QString kenji::AreaData::areaMessage() const
{
  return m_area_message.isEmpty() ? "No area message set." : m_area_message;
}

bool kenji::AreaData::sendAreaMessageOnJoin() const
{
  return m_send_area_message;
}

void kenji::AreaData::changeAreaMessage(const QString &f_newMessage_r)
{
  m_area_message = f_newMessage_r;
}

void kenji::AreaData::clearAreaMessage()
{
  changeAreaMessage(QString{});
}

bool kenji::AreaData::bgLocked() const
{
  return m_bgLocked;
}

void kenji::AreaData::toggleBgLock()
{
  m_bgLocked = !m_bgLocked;
}

bool kenji::AreaData::iniswapAllowed() const
{
  return m_iniswapAllowed;
}

void kenji::AreaData::toggleIniswap()
{
  m_iniswapAllowed = !m_iniswapAllowed;
}

bool kenji::AreaData::shownameAllowed() const
{
  return m_shownameAllowed;
}

QString kenji::AreaData::background() const
{
  return m_background;
}

void kenji::AreaData::setBackground(const QString &f_background)
{
  m_background = f_background;
  QSettings *ambience_data = ConfigManager::ambience();
  QString new_ambience = ambience_data->value(f_background + "/ambience").toString();
  if (!new_ambience.trimmed().isEmpty())
  {
    setAmbience(new_ambience);
  }
  else
  {
    setAmbience(std::nullopt);
  }
}

std::optional<QString> kenji::AreaData::side() const
{
  return m_side;
}

void kenji::AreaData::setSide(const std::optional<QString> &f_side)
{
  m_side = f_side;
}

bool kenji::AreaData::ignoreBgList()
{
  return m_ignoreBgList;
}

void kenji::AreaData::toggleIgnoreBgList()
{
  m_ignoreBgList = !m_ignoreBgList;
}

void kenji::AreaData::toggleAreaMessageJoin()
{
  m_send_area_message = !m_send_area_message;
}

void kenji::AreaData::toggleJukebox()
{
  m_jukebox = !m_jukebox;
  if (!m_jukebox)
  {
    m_jukebox_queue.clear();
    m_jukebox_timer->stop();
  }
}

void kenji::AreaData::toggleWtceAllowed()
{
  m_can_send_wtce = !m_can_send_wtce;
}

void kenji::AreaData::toggleShoutAllowed()
{
  m_can_use_shouts = !m_can_use_shouts;
}

void kenji::AreaData::toggleMedievalMode()
{
  m_medieval_mode = !m_medieval_mode;
}

QString kenji::AreaData::addJukeboxSong(const QString &f_song)
{
  if (!m_jukebox_queue.contains(f_song))
  {
    // Retrieve song information.
    const auto l_song = m_music_manager->findTrack(f_song, index());

    if (l_song && l_song->length > 0)
    {
      if (m_jukebox_queue.size() == 0)
      {
        theory::MusicChangedPacket l_music_change;
        l_music_change.track = l_song->fileName;
        l_music_change.channel = theory::MusicChannel::Music;
        m_broadcaster.broadcastToArea(l_music_change, index());
        m_jukebox_timer->start(l_song->length * 1000);
        setCurrentMusic(f_song);
      }
      m_jukebox_queue.append(f_song);
      return "Song added to Jukebox.";
    }
    else
    {
      return "Unable to add song. Duration shorter than 1.";
    }
  }
  return "Unable to add song. Song already in Jukebox.";
}

QList<int> kenji::AreaData::joinedIDs() const
{
  return m_joined_ids;
}

void kenji::AreaData::switchJukeboxSong()
{
  QString l_song_name;
  if (m_jukebox_queue.size() == 1)
  {
    l_song_name = m_jukebox_queue[0];
  }
  else
  {
    int l_random_index = QRandomGenerator::system()->bounded(m_jukebox_queue.size() - 1);
    l_song_name = m_jukebox_queue[l_random_index];

    m_jukebox_queue.remove(l_random_index);
    m_jukebox_queue.squeeze();
  }

  const auto l_song = m_music_manager->findTrack(l_song_name, index());

  theory::MusicChangedPacket l_music_change;
  l_music_change.channel = theory::MusicChannel::Music;
  if (l_song)
  {
    l_music_change.track = l_song->fileName;
  }
  m_broadcaster.broadcastToArea(l_music_change, m_index);

  if (l_song && l_song->length > 0)
  {
    m_jukebox_timer->start(l_song->length * 1000);
    setCurrentMusic(l_song->fileName);
    return;
  }

  setCurrentMusic(std::nullopt);
}

void kenji::AreaData::allowMessage()
{
  m_can_send_ic_messages = true;
}

int kenji::AreaData::getVisibleIndexByEvidenceIndex(int f_evidenceIndex, const QString &f_clientPos, bool f_isCM) const
{
  if (f_evidenceIndex < 0 || f_evidenceIndex >= m_evidence.size())
  {
    return 0; // Invalid index or not visible
  }

  int visibleCount = 0;
  for (int i = 0; i < m_evidence.size(); ++i)
  {
    const Evidence &evidence = m_evidence[i];

    // Apply the same filtering logic as in updateEvidenceList
    if (!f_isCM && m_eviMod == EvidenceMod::HIDDEN_CM)
    {
      static const QRegularExpression ownerRegex("<owner=(.*?)>");
      QRegularExpressionMatch match = ownerRegex.match(evidence.description);
      if (match.hasMatch())
      {
        QStringList owners = match.captured(1).split(",");
        if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) && !owners.contains(f_clientPos, Qt::CaseSensitivity::CaseInsensitive))
        {
          continue; // This evidence is not visible to the client
        }
      }
      // no match = show it to all
    }

    // This evidence is visible, increment counter
    ++visibleCount;
    if (i == f_evidenceIndex)
    {
      return visibleCount; // Return the visible index (1-based)
    }
  }

  return 0; // Evidence not visible to this client
}
