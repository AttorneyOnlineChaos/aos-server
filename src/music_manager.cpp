#include "music_manager.h"

#include "config_manager.h"
#include "protocol/packets/handshake_packets.h"

#include <QUrl>

kenji::MusicManager::MusicManager(Broadcaster &f_broadcaster, const QStringList &f_cdns, const QList<theory::MusicPlaylist> &f_root_list, QObject *parent)
    : QObject(parent)
    , m_broadcaster{f_broadcaster}
    , m_root_list(f_root_list)
{
  if (!f_cdns.isEmpty())
  {
    m_cdns = f_cdns;
  }
}

kenji::MusicManager::~MusicManager()
{}

QList<theory::MusicPlaylist> kenji::MusicManager::playlists(int f_area_id)
{
  QList<theory::MusicPlaylist> l_playlists;

  if (m_global_enabled.value(f_area_id))
  {
    l_playlists = m_root_list;
  }

  l_playlists.append(m_custom_lists.value(f_area_id));

  return l_playlists;
}

void kenji::MusicManager::broadcastMusicList(int f_area_id)
{
  theory::MusicListPacket l_music_list;
  l_music_list.playlists = playlists(f_area_id);
  m_broadcaster.broadcastToArea(l_music_list, f_area_id);
}

std::optional<theory::MusicTrack> kenji::MusicManager::findTrack(const QString &f_file_name, int f_area_id)
{
  const QList<theory::MusicPlaylist> l_playlists = playlists(f_area_id);
  for (const theory::MusicPlaylist &l_playlist : l_playlists)
  {
    for (const theory::MusicTrack &l_track : l_playlist.tracks)
    {
      if (l_track.fileName == f_file_name)
      {
        return l_track;
      }
    }
  }

  return std::nullopt;
}

bool kenji::MusicManager::registerArea(int f_area_id)
{
  if (m_custom_lists.contains(f_area_id))
  {
    // This area is already registered. We can't add it.
    return false;
  }
  m_custom_lists.insert(f_area_id, {});
  m_global_enabled.insert(f_area_id, true);
  return true;
}

bool kenji::MusicManager::validateSong(const QString &f_song_name, const QStringList &f_approved_cdns)
{
  const QStringList l_extensions = {".opus", ".ogg", ".mp3", ".wav"};

  // For a plain (non-URL) song name the whole string is the path; for a URL we
  // use QUrl::path() so the query string/fragment don't reach the extension check.
  QString l_path = f_song_name;

  // Check if URL formatted.
  if (f_song_name.contains("/"))
  {
    const QUrl l_url(f_song_name);

    // Only allow HTTPS/HTTP sources.
    if (l_url.scheme() != "https" && l_url.scheme() != "http")
    {
      return false;
    }

    bool l_cdn_approved = false;
    for (const QString &l_cdn : qAsConst(f_approved_cdns))
    {
      // Let QUrl extract the host so operators can write the entry with or
      // without a scheme/trailing slash (e.g. "https://cdn.discord.com/").
      // fromUserInput() is required here: the plain QUrl() ctor parses a
      // bare "cdn.discord.com" as a path and returns an empty host().
      const QString l_domain = QUrl::fromUserInput(l_cdn.trimmed()).host();
      if (!l_domain.isEmpty() && l_url.host().compare(l_domain, Qt::CaseInsensitive) == 0)
      {
        l_cdn_approved = true;
        break;
      }
    }
    if (!l_cdn_approved)
    {
      return false;
    }

    l_path = l_url.path();
  }

  for (const QString &l_suffix : qAsConst(l_extensions))
  {
    if (l_path.endsWith(l_suffix, Qt::CaseInsensitive))
    {
      return true;
    }
  }

  return false;
}

bool kenji::MusicManager::addCustomSong(const QString &f_song_name, const QString &f_real_name, theory::TrackLength f_duration, int f_area_id)
{
  // Validate if simple name.
  QString l_real_name = f_real_name;
  if (f_real_name.split(".").size() == 1)
  {
    l_real_name = l_real_name + ".opus";
  }

  if (!validateSong(l_real_name, m_cdns))
  {
    return false;
  }

  // Avoid conflicts by checking if it exists.
  if (findTrack(l_real_name, f_area_id))
  {
    return false;
  }

  theory::MusicTrack l_track;
  l_track.fileName = l_real_name;
  if (f_song_name != f_real_name)
  {
    l_track.caption = f_song_name;
  }
  if (f_duration > 0)
  {
    l_track.length = f_duration;
  }

  QList<theory::MusicPlaylist> l_custom_list = m_custom_lists.value(f_area_id);
  if (l_custom_list.isEmpty())
  {
    l_custom_list.append(theory::MusicPlaylist{});
  }

  l_custom_list.last().tracks.append(l_track);
  m_custom_lists.insert(f_area_id, l_custom_list);
  broadcastMusicList(f_area_id);
  return true;
}

bool kenji::MusicManager::addCustomCategory(const QString &f_category_name, int f_area_id)
{
  if (f_category_name.split(".").size() > 1)
  {
    return false;
  }

  // Avoid conflicts by checking if it exists.
  const QList<theory::MusicPlaylist> l_existing = playlists(f_area_id);
  for (const theory::MusicPlaylist &l_playlist : l_existing)
  {
    if (l_playlist.name == f_category_name)
    {
      return false;
    }
  }

  theory::MusicPlaylist l_playlist;
  l_playlist.name = f_category_name;

  QList<theory::MusicPlaylist> l_custom_list = m_custom_lists.value(f_area_id);
  l_custom_list.append(l_playlist);
  m_custom_lists.insert(f_area_id, l_custom_list);
  broadcastMusicList(f_area_id);
  return true;
}

bool kenji::MusicManager::removeCustomMusic(const QString &f_songcategory_name, int f_area_id)
{
  QList<theory::MusicPlaylist> l_custom_list = m_custom_lists.value(f_area_id);

  for (int i = 0; i < l_custom_list.size(); ++i)
  {
    if (l_custom_list.at(i).name == f_songcategory_name)
    {
      l_custom_list.removeAt(i);
      m_custom_lists.insert(f_area_id, l_custom_list);
      broadcastMusicList(f_area_id);
      return true;
    }

    QList<theory::MusicTrack> &l_tracks = l_custom_list[i].tracks;
    for (int j = 0; j < l_tracks.size(); ++j)
    {
      if (l_tracks.at(j).fileName == f_songcategory_name)
      {
        l_tracks.removeAt(j);
        m_custom_lists.insert(f_area_id, l_custom_list);
        broadcastMusicList(f_area_id);
        return true;
      }
    }
  }

  return false;
}

bool kenji::MusicManager::toggleCustomMusicEnabled(int f_area_id)
{
  m_global_enabled.insert(f_area_id, !m_global_enabled.value(f_area_id));
  if (m_global_enabled.value(f_area_id))
  {
    sanitiseCustomMusicList(f_area_id);
  }
  broadcastMusicList(f_area_id);
  return m_global_enabled.value(f_area_id);
}

void kenji::MusicManager::sanitiseCustomMusicList(int f_area_id)
{
  QList<theory::MusicPlaylist> l_sanitised_list;

  const QList<theory::MusicPlaylist> l_custom_list = m_custom_lists.value(f_area_id);
  for (const theory::MusicPlaylist &l_playlist : l_custom_list)
  {
    theory::MusicPlaylist l_kept;
    l_kept.name = l_playlist.name;

    for (const theory::MusicTrack &l_track : l_playlist.tracks)
    {
      if (!rootContainsTrack(l_track.fileName))
      {
        l_kept.tracks.append(l_track);
      }
    }

    if (!l_kept.name.isEmpty() || !l_kept.tracks.isEmpty())
    {
      l_sanitised_list.append(l_kept);
    }
  }

  m_custom_lists.insert(f_area_id, l_sanitised_list);
}

void kenji::MusicManager::clearCustomMusicList(int f_area_id)
{
  m_custom_lists.insert(f_area_id, {});

  broadcastMusicList(f_area_id);
}

bool kenji::MusicManager::rootContainsTrack(const QString &f_file_name) const
{
  for (const theory::MusicPlaylist &l_playlist : m_root_list)
  {
    for (const theory::MusicTrack &l_track : l_playlist.tracks)
    {
      if (l_track.fileName == f_file_name)
      {
        return true;
      }
    }
  }

  return false;
}

void kenji::MusicManager::reloadRequest()
{
  m_root_list = ConfigManager::musiclist();
  m_cdns = ConfigManager::cdnList();
}

void kenji::MusicManager::userJoinedArea(int f_area_index, int f_user_id)
{
  theory::MusicListPacket l_music_list;
  l_music_list.playlists = playlists(f_area_index);
  m_broadcaster.broadcastToPlayer(l_music_list, f_user_id);
}
