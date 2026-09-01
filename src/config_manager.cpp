#include "config_manager.h"

#include "core/logging.h"
#include "kenji_defs.h"

#include <QSqlDatabase>
#include <QSqlQuery>

kenji::ConfigManager *kenji::ConfigManager::self = nullptr;

kenji::ConfigManager::ConfigManager()
{
  Q_ASSERT(!self);
  self = this;
}

kenji::ConfigManager::~ConfigManager()
{
  Q_ASSERT(self);
  self = nullptr;
}

bool kenji::ConfigManager::verifyServerConfig()
{
  // Verify directories
  QStringList l_directories{"config/", "config/text/"};
  for (const QString &l_directory : l_directories)
  {
    if (!dirExists(QFileInfo(l_directory)))
    {
      zCritical(log::config) << l_directory + " does not exist!";
      return false;
    }
  }

  // Verify config files
  QStringList l_config_files{"config/config.ini", "config/areas.ini", "config/backgrounds.txt", "config/characters.txt", "config/music.json", "config/discord.ini", "config/text/8ball.txt", "config/text/gimp.txt", "config/text/praise.txt", "config/text/reprimands.txt", "config/text/commandhelp.json", "config/text/cdns.txt", "config/ipbans.json"};
  for (const QString &l_file : l_config_files)
  {
    if (!fileExists(QFileInfo(l_file)))
    {
      zCritical(log::config) << l_file + " does not exist!";
      return false;
    }
  }

  // Verify areas
  QSettings l_areas_ini("config/areas.ini", QSettings::IniFormat);
  if (l_areas_ini.childGroups().length() < 1)
  {
    zCritical(log::config) << "areas.ini is invalid!";
    return false;
  }

  // Read dices
  QSettings l_dice_ini("config/dice.ini", QSettings::IniFormat);
  QStringList dices = l_dice_ini.childGroups();

  for (const QString &dice : dices)
  {
    l_dice_ini.beginGroup(dice);

    int max = l_dice_ini.value("max").toInt();
    QStringList faces;

    for (int i = 1; i <= max; ++i)
    {
      QString key = QString::number(i);
      if (l_dice_ini.contains(key))
      {
        faces.append(l_dice_ini.value(key).toString());
      }
      else
      {
        zCritical(log::config) << "dice.ini max mismatch!";
        break;
      }
    }
    self->m_commands.dice_faces[dice] = faces;
    l_dice_ini.endGroup();
  }

  // Verify config settings
  self->m_settings.beginGroup("Options");
  bool ok;
  self->m_settings.value("ms_port", 27016).toInt(&ok);
  if (!ok)
  {
    zCritical(log::config) << "ms_port is not a valid port!";
    return false;
  }
  self->m_settings.value("port", 27016).toInt(&ok);
  if (!ok)
  {
    zCritical(log::config) << "port is not a valid port!";
    return false;
  }
  self->m_settings.value("secure_port", -1).toInt(&ok);
  if (!ok)
  {
    zCritical(log::config) << "secure_port is not a valid port!";
    return false;
  }

  QString l_auth = self->m_settings.value("auth", "simple").toString().toLower();
  if (!(l_auth == "simple" || l_auth == "advanced"))
  {
    zCritical(log::config) << "auth is not a valid auth type!";
    return false;
  }

  int l_soft_limit = self->m_settings.value("packet_rate_limit_soft", 10).toInt(&ok);
  if (!ok)
  {
    zCritical(log::config) << "packet_rate_limit_soft is not a valid limit!";
    return false;
  }
  if (l_soft_limit <= 0)
  {
    zWarning(log::config) << "packet_rate_limit_soft is 0 or less, warning threshold is disabled!";
  }

  int l_hard_limit = self->m_settings.value("packet_rate_limit_hard", 20).toInt(&ok);
  if (!ok)
  {
    zCritical(log::config) << "packet_rate_limit_hard is not a valid limit!";
    return false;
  }
  else if (l_soft_limit > 0 && l_hard_limit <= l_soft_limit)
  {
    zCritical(log::config) << "packet_rate_limit_hard must be greater than packet_rate_limit_soft!";
    return false;
  }
  if (l_hard_limit <= 0)
  {
    zWarning(log::config) << "packet_rate_limit_hard is 0 or less, rate limiting is disabled!";
  }

  self->m_settings.endGroup();
  self->m_commands.magic_8ball = (loadConfigFile("8ball"));
  self->m_commands.praises = (loadConfigFile("praise"));
  self->m_commands.reprimands = (loadConfigFile("reprimands"));
  self->m_commands.gimps = (loadConfigFile("gimp"));
  self->m_commands.filters = (loadConfigFile("filter"));
  self->m_commands.cdns = (loadConfigFile("cdns"));
  if (self->m_commands.cdns.isEmpty())
  {
    self->m_commands.cdns = QStringList{"cdn.discord.com"};
  }

  return true;
}

QString kenji::ConfigManager::bindIP()
{
  return self->m_settings.value("Options/bind_ip", "all").toString();
}

QStringList kenji::ConfigManager::charlist()
{
  QStringList l_charlist;
  QFile l_file("config/characters.txt");
  if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load character list:" << l_file.errorString();
    return l_charlist;
  }
  while (!l_file.atEnd())
  {
    l_charlist.append(QString::fromUtf8(l_file.readLine().trimmed()));
  }
  l_file.close();

  return l_charlist;
}

QStringList kenji::ConfigManager::backgrounds()
{
  QStringList l_backgrounds;
  QFile l_file("config/backgrounds.txt");
  if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load background list:" << l_file.errorString();
    return l_backgrounds;
  }
  while (!l_file.atEnd())
  {
    l_backgrounds.append(l_file.readLine().trimmed());
  }
  l_file.close();

  return l_backgrounds;
}

QList<theory::MusicPlaylist> kenji::ConfigManager::musiclist()
{
  QFile l_music_json("config/music.json");
  if (!l_music_json.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load musiclist:" << l_music_json.errorString();
    return {};
  }

  QJsonParseError l_error;
  QJsonDocument l_music_list_json = QJsonDocument::fromJson(l_music_json.readAll(), &l_error);
  if (!(l_error.error == QJsonParseError::NoError))
  { // Non-Terminating error.
    zWarning(log::config) << "Unable to load musiclist. The following error was encounted : " + l_error.errorString();
    return {}; // Server can still run without music.
  }

  self->m_musicList.clear();

  // Kenji expects the musiclist to be contained in a JSON array, even if its only a single category.
  QJsonArray l_Json_root_array = l_music_list_json.array();
  QJsonObject l_child_obj;
  QJsonArray l_child_array;

  for (int i = 0; i < l_Json_root_array.size(); i++)
  { // Iterate trough entire JSON file to assemble musiclist
    l_child_obj = l_Json_root_array.at(i).toObject();

    theory::MusicPlaylist l_playlist;

    // Technically not a requirement, but neat for organisation.
    l_playlist.name = l_child_obj["category"].toString();
    if (l_playlist.name.isEmpty())
    {
      zWarning(log::config) << "Category name not set. This may cause the musiclist to be displayed incorrectly.";
    }

    l_child_array = l_child_obj["songs"].toArray();
    for (int i = 0; i < l_child_array.size(); i++)
    { // Inner for loop because a category can contain multiple songs.
      QJsonObject l_song_obj = l_child_array.at(i).toObject();
      QString l_song_name = l_song_obj["name"].toString();
      QString l_real_name = l_song_obj["realname"].toString();

      theory::MusicTrack l_track;
      if (l_real_name.isEmpty())
      {
        l_track.fileName = l_song_name;
      }
      else
      {
        l_track.fileName = l_real_name;
        l_track.caption = l_song_name;
      }

      const theory::TrackLength l_song_duration = l_song_obj["length"].toVariant().toInt();
      if (l_song_duration > 0)
      {
        l_track.length = l_song_duration;
      }

      l_playlist.tracks.append(l_track);
    }

    self->m_musicList.append(l_playlist);
  }
  l_music_json.close();

  return self->m_musicList;
}

void kenji::ConfigManager::loadCommandHelp()
{
  QFile l_help_json("config/text/commandhelp.json");
  if (!l_help_json.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load help information:" << l_help_json.errorString();
    return;
  }

  QJsonParseError l_error;
  QJsonDocument l_help_list_json = QJsonDocument::fromJson(l_help_json.readAll(), &l_error);
  if (!(l_error.error == QJsonParseError::NoError))
  { // Non-Terminating error.
    zWarning(log::config) << "Unable to load help information. The following error occurred: " + l_error.errorString();
  }

  // Kenji expects the helpfile to contain multiple entires, so it always checks for an array first.
  QJsonArray l_Json_root_array = l_help_list_json.array();
  QJsonObject l_child_obj;
  QJsonArray l_names;

  for (int i = 0; i < l_Json_root_array.size(); i++)
  {
    l_child_obj = l_Json_root_array.at(i).toObject();
    l_names = l_child_obj["names"].toArray();
    QString l_usage = l_child_obj["usage"].toString();
    QString l_text = l_child_obj["text"].toString();

    for (int j = 0; j < l_names.size(); j++)
    {
      QString l_name = l_names.at(j).toString();
      if (!l_name.isEmpty())
      {
        help l_help_information = {.usage = l_usage, .text = l_text};

        self->m_commands_help.insert(l_name, l_help_information);
      }
    }
  }
}

QSettings *kenji::ConfigManager::areaData()
{
  return &self->m_areas;
}

QSettings *kenji::ConfigManager::ambience()
{
  return &self->m_ambience;
}

QStringList kenji::ConfigManager::sanitizedAreaNames()
{
  QStringList l_area_names = self->m_areas.childGroups(); // invisibly does a lexicographical sort, because Qt is great like that
  std::sort(l_area_names.begin(), l_area_names.end(), [](const QString &a, const QString &b) { return a.split(":")[0].toInt() < b.split(":")[0].toInt(); });
  QStringList l_sanitized_area_names;
  for (const QString &areaName : qAsConst(l_area_names))
  {
    QStringList l_nameSplit = areaName.split(":");
    l_nameSplit.removeFirst();
    QString l_area_name_sanitized = l_nameSplit.join(":");
    l_sanitized_area_names.append(l_area_name_sanitized);
  }
  return l_sanitized_area_names;
}

QStringList kenji::ConfigManager::iprangeBans()
{
  QFile l_json_file("config/ipbans.json");
  if (!l_json_file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load IP range bans:" << l_json_file.errorString();
    return {};
  }

  QJsonParseError l_error;
  QJsonDocument l_ip_bans = QJsonDocument::fromJson(l_json_file.readAll(), &l_error);
  if (l_error.error != QJsonParseError::NoError)
  {
    zDebug(log::config) << "Unable to parse JSON file. Error:" << l_error.errorString();
    return {};
  }

  QJsonObject l_json_obj = l_ip_bans.object();

  QStringList l_range_bans;
  l_range_bans.append(l_json_obj["ip_range"].toVariant().toStringList());

  if (QFile::exists("storage/asn.sqlite3"))
  {
    QSqlDatabase asn_db = QSqlDatabase::addDatabase("QSQLITE", "ASN");
    asn_db.setDatabaseName("storage/asn.sqlite3");
    asn_db.open();

    // This is a dumb hack. Idk how else I can do this, but who gives a shit?
    QSqlQuery query("SELECT ip FROM maxmind WHERE asn in (" + l_json_obj["asn"].toVariant().toStringList().join(",") + ")", asn_db);
    query.exec();
    while (query.next())
    {
      l_range_bans.append(query.value(0).toString());
    }
    asn_db.close();
  }
  l_range_bans.removeDuplicates();
  return l_range_bans;
}

void kenji::ConfigManager::reloadSettings()
{
  self->m_settings.sync();
  self->m_discord.sync();
  self->m_logtext.sync();
}

QStringList kenji::ConfigManager::loadConfigFile(const QString &filename)
{
  QStringList stringlist;
  QFile l_file("config/text/" + filename + ".txt");
  if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    zWarning(log::config) << "Failed to load config file" << filename << ":" << l_file.errorString();
    return stringlist;
  }
  while (!(l_file.atEnd()))
  {
    stringlist.append(l_file.readLine().trimmed());
  }
  l_file.close();
  return stringlist;
}

int kenji::ConfigManager::maxPlayers()
{
  bool ok;
  int l_players = self->m_settings.value("Options/max_players", 100).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "max_players is not an int!";
    l_players = 100;
  }
  return l_players;
}

int kenji::ConfigManager::serverPort()
{
  if (self->m_settings.contains("Options/webao_port"))
  {
    zWarning(log::config) << "webao_port is deprecated, use port instead";
    return self->m_settings.value("Options/webao_port", 27016).toInt();
  }

  return self->m_settings.value("Options/port", 27016).toInt();
}

int kenji::ConfigManager::securePort()
{
  return self->m_settings.value("Options/secure_port", -1).toInt();
}

QString kenji::ConfigManager::serverDescription()
{
  return self->m_settings.value("Options/server_description", "This is my flashy new server!").toString();
}

QString kenji::ConfigManager::serverName()
{
  return self->m_settings.value("Options/server_name", "An Unnamed Server").toString();
}

QString kenji::ConfigManager::serverNickname()
{
  QString l_tag = self->m_settings.value("Options/server_nickname").toString();
  return l_tag.isEmpty() ? serverName() : l_tag;
}

QString kenji::ConfigManager::motd()
{
  return self->m_settings.value("Options/motd", "MOTD not set").toString();
}

kenji::DataTypes::AuthType kenji::ConfigManager::authType()
{
  QString l_auth = self->m_settings.value("Options/auth", "simple").toString().toUpper();
  return toDataType<DataTypes::AuthType>(l_auth);
}

QString kenji::ConfigManager::modpass()
{
  return self->m_settings.value("Options/modpass", "changeme").toString();
}

int kenji::ConfigManager::logBuffer()
{
  bool ok;
  int l_buffer = self->m_settings.value("Options/logbuffer", 500).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "logbuffer is not an int!";
    l_buffer = 500;
  }
  return l_buffer;
}

kenji::DataTypes::LogType kenji::ConfigManager::loggingType()
{
  QString l_log = self->m_settings.value("Options/logging", "modcall").toString().toUpper();
  return toDataType<DataTypes::LogType>(l_log);
}

int kenji::ConfigManager::maxStatements()
{
  bool ok;
  int l_max = self->m_settings.value("Options/maximum_statements", 10).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "maximum_statements is not an int!";
    l_max = 10;
  }
  return l_max;
}
int kenji::ConfigManager::multiClientLimit()
{
  bool ok;
  int l_limit = self->m_settings.value("Options/multiclient_limit", 15).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "multiclient_limit is not an int!";
    l_limit = 15;
  }
  return l_limit;
}

int kenji::ConfigManager::maxNameLength()
{
  bool ok;
  int l_max = self->m_settings.value("Options/maximum_name_length", 30).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "maximum_name_length is not an int!";
    l_max = 30;
  }
  return l_max;
}

int kenji::ConfigManager::maxIcNameLength()
{
  bool ok;
  int l_max = self->m_settings.value("Options/maximum_ic_name_length", 30).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "maximum_ic_name_length is not an int!";
    l_max = 30;
  }
  return l_max;
}

int kenji::ConfigManager::maxTextLength()
{
  bool ok;
  int l_max = self->m_settings.value("Options/maximum_text_length", 256).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "maximum_text_length is not an int!";
    l_max = 256;
  }
  return l_max;
}

int kenji::ConfigManager::maxIcTextLength()
{
  bool ok;
  int l_max = self->m_settings.value("Options/maximum_ic_text_length", 256).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "maximum_ic_text_length is not an int!";
    l_max = 256;
  }
  return l_max;
}

int kenji::ConfigManager::messageFloodguard()
{
  bool ok;
  int l_flood = self->m_settings.value("Options/message_floodguard", 250).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "message_floodguard is not an int!";
    l_flood = 250;
  }
  return l_flood;
}

int kenji::ConfigManager::globalMessageFloodguard()
{
  bool ok;
  int l_flood = self->m_settings.value("Options/global_message_floodguard", 0).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "global_message_floodguard is not an int!";
    l_flood = 0;
  }
  return l_flood;
}

int kenji::ConfigManager::packetRateLimitSoft()
{
  bool ok;
  int l_limit = self->m_settings.value("Options/packet_rate_limit_soft", 10).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "packet_rate_limit_soft is not an int!";
    l_limit = 10;
  }
  return l_limit;
}

int kenji::ConfigManager::packetRateLimitHard()
{
  bool ok;
  int l_limit = self->m_settings.value("Options/packet_rate_limit_hard", 20).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "packet_rate_limit_hard is not an int!";
    l_limit = 20;
  }
  return l_limit;
}

int kenji::ConfigManager::maxPacketSize()
{
  bool ok;
  int l_size = self->m_settings.value("Options/max_packet_size", 65536).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "max_packet_size is not an int!";
    l_size = 65536;
  }
  return l_size;
}

int kenji::ConfigManager::modcallReasonLimit()
{
  bool ok;
  int l_limit = self->m_settings.value("Options/modcall_reason_limit", 255).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "modcall_reason_limit is not an int!";
    l_limit = 255;
  }
  return l_limit;
}

int kenji::ConfigManager::infoRateLimit()
{
  bool ok;
  int l_limit = self->m_settings.value("Options/info_rate_limit", 100).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "info_rate_limit is not an int!";
    l_limit = 100;
  }
  return l_limit;
}

int kenji::ConfigManager::handshakeTimeout()
{
  bool ok;
  int l_timeout = self->m_settings.value("Options/handshake_timeout", 10).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "handshake_timeout is not an int!";
    l_timeout = 10;
  }
  return l_timeout;
}

int kenji::ConfigManager::sessionTimeout()
{
  bool ok;
  int l_timeout = self->m_settings.value("Options/session_timeout", 180).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "session_timeout is not an int!";
    l_timeout = 180;
  }
  return l_timeout;
}

int kenji::ConfigManager::connectionHeadroom()
{
  bool ok;
  int l_headroom = self->m_settings.value("Options/connection_headroom", 20).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "connection_headroom is not an int!";
    l_headroom = 20;
  }
  return l_headroom;
}

QUrl kenji::ConfigManager::assetUrl()
{
  QByteArray l_url = self->m_settings.value("Options/asset_url", "").toString().toUtf8();
  if (QUrl(l_url).isValid())
  {
    return QUrl(l_url);
  }
  else
  {
    zWarning(log::config) << "asset_url is not a valid url!";
    return QUrl(NULL);
  }
}

int kenji::ConfigManager::diceMaxValue()
{
  bool ok;
  int l_value = self->m_settings.value("Dice/max_value", 100).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "max_value is not an int!";
    l_value = 100;
  }
  return l_value;
}

int kenji::ConfigManager::diceMaxDice()
{
  bool ok;
  int l_dice = self->m_settings.value("Dice/max_dice", 100).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "max_dice is not an int!";
    l_dice = 100;
  }
  return l_dice;
}

bool kenji::ConfigManager::discordWebhookEnabled()
{
  return self->m_discord.value("Discord/webhook_enabled", false).toBool();
}

bool kenji::ConfigManager::discordModcallWebhookEnabled()
{
  return self->m_discord.value("Discord/webhook_modcall_enabled", false).toBool();
}

QString kenji::ConfigManager::discordModcallWebhookUrl()
{
  return self->m_discord.value("Discord/webhook_modcall_url", "").toString();
}

QString kenji::ConfigManager::discordModcallWebhookContent()
{
  return self->m_discord.value("Discord/webhook_modcall_content", "").toString();
}

bool kenji::ConfigManager::discordModcallWebhookSendFile()
{
  return self->m_discord.value("Discord/webhook_modcall_sendfile", false).toBool();
}

bool kenji::ConfigManager::discordBanWebhookEnabled()
{
  return self->m_discord.value("Discord/webhook_ban_enabled", false).toBool();
}

QString kenji::ConfigManager::discordBanWebhookUrl()
{
  return self->m_discord.value("Discord/webhook_ban_url", "").toString();
}

QString kenji::ConfigManager::discordWebhookColor()
{
  const QString l_default_color = "13312842";
  QString l_color = self->m_discord.value("Discord/webhook_color", l_default_color).toString();
  if (l_color.isEmpty())
  {
    return l_default_color;
  }
  else
  {
    return l_color;
  }
}

bool kenji::ConfigManager::passwordRequirements()
{
  return self->m_settings.value("Password/password_requirements", true).toBool();
}

int kenji::ConfigManager::passwordMinLength()
{
  bool ok;
  int l_min = self->m_settings.value("Password/pass_min_length", 8).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "pass_min_length is not an int!";
    l_min = 8;
  }
  return l_min;
}

int kenji::ConfigManager::passwordMaxLength()
{
  bool ok;
  int l_max = self->m_settings.value("Password/pass_max_length", 0).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "pass_max_length is not an int!";
    l_max = 0;
  }
  return l_max;
}

bool kenji::ConfigManager::passwordRequireMixCase()
{
  return self->m_settings.value("Password/pass_required_mix_case", true).toBool();
}

bool kenji::ConfigManager::passwordRequireNumbers()
{
  return self->m_settings.value("Password/pass_required_numbers", true).toBool();
}

bool kenji::ConfigManager::passwordRequireSpecialCharacters()
{
  return self->m_settings.value("Password/pass_required_special", true).toBool();
}

bool kenji::ConfigManager::passwordCanContainUsername()
{
  return self->m_settings.value("Password/pass_can_contain_username", false).toBool();
}

QString kenji::ConfigManager::LogText(const QString &f_logtype)
{
  return self->m_logtext.value("LogConfiguration/" + f_logtype, "").toString();
}

int kenji::ConfigManager::afkTimeout()
{
  bool ok;
  int l_afk = self->m_settings.value("Options/afk_timeout", 300).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "afk_timeout is not an int!";
    l_afk = 300;
  }
  return l_afk;
}

int kenji::ConfigManager::syncInterval()
{
  bool ok;
  int l_interval = self->m_settings.value("Options/sync_interval", 5).toInt(&ok);
  if (!ok)
  {
    zWarning(log::config) << "sync_interval is not an int!";
    l_interval = 5;
  }
  return qMax(1, l_interval);
}

void kenji::ConfigManager::setAuthType(const DataTypes::AuthType f_auth)
{
  self->m_settings.setValue("Options/auth", fromDataType<DataTypes::AuthType>(f_auth).toLower());
}

QStringList kenji::ConfigManager::diceFaces(const QString &f_name)
{
  return self->m_commands.dice_faces[f_name];
}

QStringList kenji::ConfigManager::magic8BallAnswers()
{
  return self->m_commands.magic_8ball;
}

QStringList kenji::ConfigManager::praiseList()
{
  return self->m_commands.praises;
}

QStringList kenji::ConfigManager::reprimandsList()
{
  return self->m_commands.reprimands;
}

QStringList kenji::ConfigManager::gimpList()
{
  return self->m_commands.gimps;
}

QStringList kenji::ConfigManager::filterList()
{
  return self->m_commands.filters;
}

QStringList kenji::ConfigManager::cdnList()
{
  return self->m_commands.cdns;
}

bool kenji::ConfigManager::publishServerEnabled()
{
  return self->m_settings.value("Advertiser/advertise", "true").toBool();
}

QUrl kenji::ConfigManager::serverlistURL()
{
  return self->m_settings.value("Advertiser/ms_ip", "").toUrl();
}

QString kenji::ConfigManager::serverDomainName()
{
  return self->m_settings.value("Advertiser/hostname", "").toString();
}

bool kenji::ConfigManager::advertiseWSProxy()
{
  return self->m_settings.value("Advertiser/cloudflare_enabled", "false").toBool();
}

kenji::ConfigManager::help kenji::ConfigManager::commandHelp(const QString &f_command_name)
{
  return self->m_commands_help.value(f_command_name);
}

void kenji::ConfigManager::setMotd(const QString &f_motd)
{
  self->m_settings.setValue("Options/motd", f_motd);
}

bool kenji::ConfigManager::fileExists(const QFileInfo &f_file)
{
  return (f_file.exists() && f_file.isFile());
}

bool kenji::ConfigManager::dirExists(const QFileInfo &f_dir)
{
  return (f_dir.exists() && f_dir.isDir());
}
