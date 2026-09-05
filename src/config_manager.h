#pragma once

#include "data_types.h"
#include "game/music.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QSettings>
#include <QUrl>

namespace kenji
{
/**
 * @brief The config file handler class.
 */
class ConfigManager
{
public:
  ConfigManager();
  ~ConfigManager();

  /**
   * @brief Verifies the server configuration, confirming all required files/directories exist and are valid.
   *
   * @return True if the server configuration was verified, false otherwise.
   */
  static bool verifyServerConfig();

  /**
   * @brief Returns the IP the server binds to.
   */
  static QString bindIP();

  /**
   * @brief Returns the character list of the server..
   */
  static QStringList charlist();

  /**
   * @brief Returns the a QStringList of the available backgrounds..
   */
  static QStringList backgrounds();

  /**
   * @brief Returns the musiclist.
   */
  static QList<theory::MusicPlaylist> musiclist();

  /**
   * @brief Loads help information into m_help_information..
   */
  static void loadCommandHelp();

  /**
   * @brief Returns a pointer to the QSettings object which contains the area configuration..
   */
  static QSettings *areaData();

  /**
   * @brief Returns a pointer to the QSettings object which contains the ambience configuration..
   */
  static QSettings *ambience();

  /**
   * @brief Returns a sanitized QStringList of the areas..
   */
  static QStringList sanitizedAreaNames();

  /**
   * @brief Returns a list of the IPrange bans..
   */
  static QStringList iprangeBans();

  /**
   * @brief Returns the maximum number of players the server will allow..
   */
  static int maxPlayers();

  /**
   * @brief Returns the port to listen for connections on..
   */
  static int serverPort();

  /**
   * @brief Returns the SSL port to listen for connections on..
   */
  static int securePort();

  /**
   * @brief Returns the server description..
   */
  static QString serverDescription();

  /**
   * @brief Returns the server name..
   */
  static QString serverName();

  /**
   * @brief Returns the server's nickname.
   */
  static QString serverNickname();

  /**
   * @brief Returns the server's Message of the Day..
   */
  static QString motd();

  /**
   * @brief Returns the server's authorization type..
   */
  static DataTypes::AuthType authType();

  /**
   * @brief Returns the server's moderator password..
   */
  static QString modpass();

  /**
   * @brief Returns the server's log buffer length..
   */
  static int logBuffer();

  /**
   * @brief Returns the server's logging type..
   */
  static DataTypes::LogType loggingType();

  /**
   * @brief Returns true if the server should advertise to the master server..
   */
  static int maxStatements();

  /**
   * @brief Returns the maximum number of permitted connections from the same IP..
   */
  static int multiClientLimit();

  /**
   * @brief Returns the maximum number of characters a ysername can contain.
   */
  static int maxNameLength();

  /**
   * @brief Returns the maximum number of characters a character name (showname) can contain.
   */
  static int maxCharacterNameLength();

  /**
   * @brief Returns the maximum number of characters an OOC message can contain.
   */
  static int maxMessageLength();

  /**
   * @brief Returns the maximum number of characters an IC message can contain.
   */
  static int maxIcMessageLength();

  static int maxEvidenceNameLength();
  static int maxEvidenceDescriptionLength();

  static int maxInventorySize();
  static int maxPersonalInventorySize();

  /**
   * @brief Returns the duration of the message floodguard..
   */
  static int messageFloodguard();

  /**
   * @brief Returns the duration of the global message floodguard..
   */
  static int globalMessageFloodguard();

  /**
   * @brief Returns the packet count limit for the warning threshold..
   */
  static int packetRateLimitSoft();

  /**
   * @brief Returns the packet count limit for the disconnection threshold..
   */
  static int packetRateLimitHard();

  /**
   * @brief Returns the maximum accepted size of a single packet, in bytes.
   */
  static int maxPacketSize();

  static int modcallReasonLimit();

  /**
   * @brief Returns the maximum number of /info replies allowed per addresses.
   */
  static int infoRateLimit();

  /**
   * @brief Returns how long a connection may idle without completing the handshake before it is dropped, in seconds.
   */
  static int handshakeTimeout();

  static int sessionTimeout();

  static int connectionHeadroom();

  /**
   * @brief Returns the URL where the server should retrieve remote assets from..
   */
  static QUrl assetUrl();

  /**
   * @brief Returns the maximum number of sides dice can have..
   */
  static int diceMaxValue();

  /**
   * @brief Returns the maximum number of dice that can be rolled at once..
   */
  static int diceMaxDice();

  /**
   * @brief Returns true if the discord webhook integration is enabled..
   */
  static bool discordWebhookEnabled();

  /**
   * @brief Returns true if the discord modcall webhook is enabled..
   */
  static bool discordModcallWebhookEnabled();

  /**
   * @brief Returns the discord webhook URL..
   */
  static QString discordModcallWebhookUrl();

  /**
   * @brief Returns the discord webhook content..
   */
  static QString discordModcallWebhookContent();

  /**
   * @brief Returns true if the discord webhook should send log files..
   */
  static bool discordModcallWebhookSendFile();

  /**
   * @brief Returns true if the discord ban webhook is enabled..
   */
  static bool discordBanWebhookEnabled();

  /**
   * @brief Returns the Discord Ban Webhook URL..
   */
  static QString discordBanWebhookUrl();

  /**
   * @brief Returns a user configurable color code for the embeed object.s.
   */
  static QString discordWebhookColor();

  /**
   * @brief Returns true if password requirements should be enforced..
   */
  static bool passwordRequirements();

  /**
   * @brief Returns the minimum length passwords must be..
   */
  static int passwordMinLength();

  /**
   * @brief Returns the maximum length passwords can be, or `0` for unlimited length..
   */
  static int passwordMaxLength();

  /**
   * @brief Returns true if passwords must be mixed case..
   */
  static bool passwordRequireMixCase();

  /**
   * @brief Returns true is passwords must contain one or more numbers..
   */
  static bool passwordRequireNumbers();

  /**
   * @brief Returns true if passwords must contain one or more special characters...
   */
  static bool passwordRequireSpecialCharacters();

  /**
   * @brief Returns true if passwords can contain the username..
   */
  static bool passwordCanContainUsername();

  /**
   * @brief Returns the logstring for the specified logtype.
   *
   * @param Name of the logstring we want..
   */
  static QString LogText(const QString &f_logtype);

  /**
   * @brief Returns the duration before a client is considered AFK..
   */
  static int afkTimeout();

  /**
   * @brief Returns the duration between two synchronisation passes, in seconds.
   */
  static int syncInterval();

  /**
   * @brief Returns a list of dice faces..
   */
  static QStringList diceFaces(const QString &f_name);

  /**
   * @brief Returns a list of magic 8 ball answers..
   */
  static QStringList magic8BallAnswers();

  /**
   * @brief Returns a list of praises..
   */
  static QStringList praiseList();

  /**
   * @brief Returns a list of reprimands..
   */
  static QStringList reprimandsList();

  /**
   * @brief Returns the server gimp list..
   */
  static QStringList gimpList();

  /**
   * @brief Returns the server regex filter list.
   */
  static QStringList filterList();

  /**
   * @brief Returns the server approved domain list..
   */
  static QStringList cdnList();

  /**
   * @brief Returns if the advertiser is enabled to advertise on ms3.
   */
  static bool publishServerEnabled();

  /**
   * @brief Returns the IP or URL of the masterserver.
   */
  static QUrl serverlistURL();

  /**
   * @brief Returns an optional hostname paramemter for the advertiser.
   * If used allows user to set a custom IP or domain name.
   */
  static QString serverDomainName();

  /**
   * @brief Returns a dummy port instead of the real port
   * @return
   */
  static bool advertiseWSProxy();

  /**
   * @brief A struct that contains the help information for a command.
   *        It's split in the syntax and the explanation text.
   */
  struct help
  {
    QString usage;
    QString text;
  };

  /**
   * @brief Returns a struct with the help information of the command..
   */
  static help commandHelp(const QString &f_command_name);

  /**
   * @brief Sets the server's authorization type.
   *
   * @param f_auth The auth type to set.
   */
  static void setAuthType(const DataTypes::AuthType f_auth);

  /**
   * @brief Sets the server's Message of the Day.
   *
   * @param f_motd The MOTD to set.
   */
  static void setMotd(const QString &f_motd);

  /**
   * @brief Reload the server configuration.
   */
  static void reloadSettings();

private:
  static ConfigManager *self;

  /**
   * @brief Checks if a file exists and is valid.
   *
   * @param file The file to check.
   *
   * @return True if the file exists and is valid, false otherwise.
   */
  static bool fileExists(const QFileInfo &file);

  /**
   * @brief Checks if a directory exists and is valid.
   *
   * @param file The directory to check.
   *
   * @return True if the directory exists and is valid, false otherwise.
   */
  static bool dirExists(const QFileInfo &dir);

  /**
   * @brief A struct for storing QStringLists loaded from command configuration files.
   */
  struct CommandSettings
  {
    QHash<QString, QStringList> dice_faces; //!< Contains customizable dices, found in config/dice.ini
    QStringList magic_8ball;                //!< Contains answers for /8ball, found in config/text/8ball.txt
    QStringList praises;                    //!< Contains command praises, found in config/text/praises.txt
    QStringList reprimands;                 //!< Contains command reprimands, found in config/text/reprimands.txt
    QStringList gimps;                      //!< Contains phrases for /gimp, found in config/text/gimp.txt
    QStringList filters;                    //!< Contains filter regex, found in config/text/filter.txt
    QStringList cdns;                       //!< Contains domains for custom song validation, found in config/text/cdns.txt
  };

  /**
   * @brief Contains the settings required for various commands.
   */
  CommandSettings m_commands;

  /**
   * @brief Stores all server configuration values.
   */
  QSettings m_settings{"config/config.ini", QSettings::IniFormat};

  /**
   * @brief Stores all discord webhook configuration values.
   */
  QSettings m_discord{"config/discord.ini", QSettings::IniFormat};

  /**
   * @brief Stores all of the area valus.
   */
  QSettings m_areas{"config/areas.ini", QSettings::IniFormat};

  /**
   * @brief Stores all adjustable logstrings.
   */
  QSettings m_logtext{"config/text/logtext.ini", QSettings::IniFormat};

  /**
   * @brief Stores all adjustable logstrings.
   */
  QSettings m_ambience{"config/ambience.ini", QSettings::IniFormat};

  /**
   * @brief Contains the musiclist with time durations.
   */
  QList<theory::MusicPlaylist> m_musicList;

  /**
   * @brief QHash containing the help information for all commands registered to the server.
   */
  QHash<QString, help> m_commands_help;

  /**
   * @brief Returns a stringlist with the contents of a .txt file from config/text/.
   *
   * @param Name of the file to load.
   */
  static QStringList loadConfigFile(const QString &filename);
};
} // namespace kenji
