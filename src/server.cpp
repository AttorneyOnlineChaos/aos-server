#include "server.h"

#include "acl_roles_handler.h"
#include "ao_client.h"
#include "area_data.h"
#include "badge/badge_gatekeeper.h"
#include "command_extension.h"
#include "config_manager.h"
#include "core/logging.h"
#include "discord.h"
#include "inventory/area_inventory_handle.h"
#include "kenji_defs.h"
#include "kenji_info.h"
#include "logger/u_logger.h"
#include "music_manager.h"
#include "network/cargo_socket.h"
#include "protocol/packets/handshake_packets.h"
#include "protocol/packets/moderation_packets.h"
#include "protocol/protocol_info.h"
#include "protocol/server_info.h"
#include "protocol/server_settings.h"
#include "server/host_error.h"
#include "server/host_server.h"
#include "server/master_publisher.h"

#include <QCryptographicHash>
#include <QDir>

kenji::Server::Server(int p_ws_port, const theory::PacketFactory &f_packet_factory, QObject *parent)
    : QObject(parent)
    , m_port(p_ws_port)
    , m_packet_factory(f_packet_factory)
    , m_player_count(0)
{
  m_sync_timer = new QTimer(this);
  m_sync_timer->setInterval(ConfigManager::syncInterval() * 1000);
  connect(m_sync_timer, &QTimer::timeout, this, &Server::synchronize);
  m_sync_timer->start();

  m_global_timer = new Timer(0, this);

  connect(m_global_timer, &Timer::stateChanged, this, [this] {
    broadcast(makeTimerPacket(*m_global_timer, theory::TimerPacket::State));
    broadcast(makeTimerPacket(*m_global_timer, theory::TimerPacket::Tick));
  });

  connect(m_global_timer, &Timer::visibilityChanged, this, [this] { broadcast(makeTimerPacket(*m_global_timer, theory::TimerPacket::Visibility)); });

  medieval_parser = theory::makeUnique<MedievalParser>();

  acl_roles_handler = new ACLRolesHandler(this);
  acl_roles_handler->loadFile("config/acl_roles.ini");

  command_extension_collection = new CommandExtensionCollection(this);
  command_extension_collection->setCommandNameWhitelist(AOClient::COMMANDS.keys());
  command_extension_collection->loadFile("config/command_extensions.ini");

  // We create it, even if its not used later on.
  discord = theory::makeUnique<Discord>();

  logger = new ULogger(this);
}

bool kenji::Server::start()
{
  QString bind_ip = ConfigManager::bindIP();
  QHostAddress bind_addr;
  if (bind_ip == "all")
  {
    bind_addr = QHostAddress::Any;
  }
  else
  {
    bind_addr = QHostAddress(bind_ip);
  }

  if (bind_addr.protocol() != QAbstractSocket::IPv4Protocol && bind_addr.protocol() != QAbstractSocket::IPv6Protocol && bind_addr != QHostAddress::Any)
  {
    zDebug(log::network) << bind_ip << "is an invalid IP address to listen on! Server not starting, check your config.";
  }

  _database = theory::makeUnique<ServerDatabase>(QStringLiteral("storage"));
  if (const auto error = _database->open())
  {
    zWarning(log::main) << QStringLiteral("refusing to start: server database: %1").arg(error->toString());
    return false;
  }

  _users = theory::makeUnique<theory::UserDatabase>(QStringLiteral("storage"), ConfigManager::userTokenTtl());
  if (const auto error = _users->open())
  {
    zWarning(log::main) << QStringLiteral("refusing to start: user database: %1").arg(error->toString());
    return false;
  }

  connect(_users.get(), &theory::UserDatabase::userAdded, logger, &ULogger::logRegistration);

  _guests = theory::makeUnique<theory::GuestTokenRegistry>();

  _badgeEngine = theory::makeUnique<theory::BadgeServerEngine>(QDir{ConfigManager::badgePluginDataDirectory()}, ConfigManager::badgeRedirectOrigin(), QStringLiteral("/auth"));
  const QList<theory::PluginError> errors = _badgeEngine->loadPlugins(ConfigManager::badgePluginsDirectory());
  if (!errors.isEmpty())
  {
    for (const theory::PluginError &error : errors)
    {
      zWarning(log::main) << QStringLiteral("refusing to start: badge plugin: %1").arg(error.toString());
    }

    return false;
  }

  if (const auto error = _badgeEngine->setBadgeIds(ConfigManager::badgeIds()))
  {
    zWarning(log::main) << QStringLiteral("refusing to start: badge: %1").arg(error->toString());
    return false;
  }

  _guard.setLimit(ConfigManager::challengeAttempts(), ConfigManager::challengeAttemptWindow());
  _gateway = theory::makeUnique<theory::BadgeGateway>(theory::BadgeGatekeeper::Context{*_badgeEngine, *_users, *_guests, _guard, ConfigManager::badgeRoundLimit(), ConfigManager::badgeSelectTimeout(), ConfigManager::challengeTimeout()});

  theory::HostSettings hostSettings;
  hostSettings.bindAddress = bind_addr;
  hostSettings.port = m_port;
  hostSettings.useTls = ConfigManager::useTls();
  hostSettings.tlsCertificate = ConfigManager::tlsCertificate();
  hostSettings.tlsPrivateKey = ConfigManager::tlsPrivateKey();
  hostSettings.authPage = ConfigManager::badgeAuthPage();
  hostSettings.maxPendingConnections = ConfigManager::maxPlayers() + ConfigManager::connectionHeadroom();
  hostSettings.infoRateLimit = ConfigManager::infoRateLimit();
  hostSettings.infoRateWindow = ConfigManager::infoRateWindow();
  hostSettings.maxMessageSize = ConfigManager::maxPacketSize();

  _host = theory::makeUnique<theory::HostServer>(*_badgeEngine);
  _host->setInfoHandler([this] { return serverInfo(); });
  _host->setJoinHandler([](const QHostAddress &) { return true; });
  _host->setSocketHandler([this](QWebSocket *socket, const QHostAddress &clientAddress) { acceptConnection(socket, clientAddress); });
  if (const auto error = _host->start(hostSettings))
  {
    zCritical(log::network) << QStringLiteral("refusing to start: %1").arg(error->toString());
    return false;
  }

  const quint16 l_port = _host->port();
  zInfo(log::network) << "Server listening on" << l_port;

  // Checks if any Discord webhooks are enabled.
  handleDiscordIntegration();

  // Construct modern advertiser if enabled in config
  if (ConfigManager::publishServerEnabled())
  {
    const QUrl l_master_url = ConfigManager::serverlistURL();
    if (!l_master_url.isValid())
    {
      zWarning(log::main) << QStringLiteral("not advertising: Advertiser/ms_ip: '%1'").arg(l_master_url.toString());
    }
    else
    {
      theory::MasterSettings l_master;
      l_master.masterUrl = l_master_url;
      if (!ConfigManager::serverDomainName().trimmed().isEmpty())
      {
        l_master.hostname = ConfigManager::serverDomainName();
      }

      l_master.port = l_port;
      l_master.useTls = ConfigManager::useTls();
      _publisher = theory::makeUnique<theory::MasterPublisher>();
      _publisher->setInfoHandler([this] { return serverInfo(); });
      _publisher->start(l_master);
      connect(this, &Server::updateHTTPConfiguration, _publisher.get(), &theory::MasterPublisher::publish);
    }
  }

  // Get characters from config file
  const QStringList l_charlist = ConfigManager::charlist();
  for (const QString &i_char_name : l_charlist)
  {
    const theory::CharacterId l_char_id = theory::CharacterId(i_char_name);
    if (m_characters.contains(l_char_id))
    {
      zWarning(log::config) << "skipping unusable or duplicate character entry:" << i_char_name;
      continue;
    }

    m_characters.append(l_char_id);
  }

  // Get backgrounds from config file
  m_backgrounds = ConfigManager::backgrounds();

  // Build our music manager.

  music_manager = new MusicManager(*this, ConfigManager::cdnList(), ConfigManager::musiclist(), this);
  connect(this, &Server::reloadRequest, music_manager, &MusicManager::reloadRequest);

  m_inventory_registry = theory::makeUnique<InventoryRegistry>();
  m_client_registry = theory::makeUnique<AOClientRegistry>(*this, *logger, *music_manager, *m_inventory_registry, ConfigManager::maxPlayers());
  connect(m_client_registry.get(), &AOClientRegistry::clientAdded, this, &Server::increasePlayerCount);
  connect(m_client_registry.get(), &AOClientRegistry::clientRemoved, this, &Server::decreasePlayerCount);
  connect(m_client_registry.get(), &AOClientRegistry::clientAdded, this, [this](theory::PlayerId f_player_id) {
    AOClient *l_client = m_client_registry->client(f_player_id);
    m_game_observers.insert(f_player_id, new ClientGameObserver(*l_client, *m_client_registry, *m_inventory_registry, *this, l_client));
  });
  connect(m_client_registry.get(), &AOClientRegistry::aboutToRemoveClient, this, [this](theory::PlayerId f_player_id) { delete m_game_observers.take(f_player_id); });
  m_session_registry = theory::makeUnique<SessionRegistry>(*m_client_registry, *_guests);
  m_connection_pool = theory::makeUnique<ConnectionPool>(*_gateway, *m_session_registry, *_database);

  // Assembles the area list
  m_area_names = ConfigManager::sanitizedAreaNames();
  for (theory::AreaId i = 0; i < m_area_names.length(); i++)
  {
    QString area_name = QString::number(i) + ":" + m_area_names[i];
    const theory::InventoryId inventory_id = m_inventory_registry->add(theory::makeShared<AreaInventoryHandle>(i, *this));
    AreaData *l_area = new AreaData(area_name, i, inventory_id, music_manager, *this);
    m_areas.insert(i, l_area);
    connect(l_area, &AreaData::userJoinedArea, music_manager, &MusicManager::userJoinedArea);
    connect(l_area, &AreaData::ownersChanged, this, [this, l_area] {
      if (l_area->owners().isEmpty())
      {
        m_inventory_registry->clear(l_area->inventoryId);
      }
    });
    music_manager->registerArea(i);
  }

  // Loads the command help information. This is not stored inside the server.
  ConfigManager::loadCommandHelp();

  // Get IP bans
  m_ipban_list = ConfigManager::iprangeBans();

  // Rate-Limiter for IC-Chat
  m_message_floodguard_timer = new QTimer(this);
  m_message_floodguard_timer->setSingleShot(true);
  connect(m_message_floodguard_timer, &QTimer::timeout, this, &Server::allowMessage);

  return true;
}

QList<kenji::AOClient *> kenji::Server::getClients()
{
  return m_client_registry->clients();
}

void kenji::Server::acceptConnection(QWebSocket *socket, const QHostAddress &clientAddress)
{
  theory::Shared<theory::CargoSocket> l_socket = theory::makeSharedObject<theory::CargoSocket>(socket);
  l_socket->setFactory(&m_packet_factory);

  auto refuse = [&l_socket](theory::ErrorPacket::Code code, const QString &what = QString()) {
    theory::ErrorPacket packet;
    packet.code = code;
    packet.what = what;
    l_socket->shipPacket(packet);
    l_socket->close();
  };

  if (m_connection_pool->count() >= m_client_registry->capacity() + ConfigManager::connectionHeadroom())
  {
    refuse(theory::ErrorPacket::ServerFull);
    return;
  }

  int multiclient_count = 1 + m_client_registry->countByAddress(clientAddress);
  bool is_at_multiclient_limit = multiclient_count > ConfigManager::multiClientLimit() && !clientAddress.isLoopback();
  if (is_at_multiclient_limit)
  {
    refuse(theory::ErrorPacket::ServerFull);
    return;
  }

  QHostAddress l_remote_ip = clientAddress;
  if (l_remote_ip.protocol() == QAbstractSocket::IPv6Protocol)
  {
    l_remote_ip = parseToIPv4(l_remote_ip);
  }

  if (isIPBanned(l_remote_ip))
  {
    refuse(theory::ErrorPacket::Banned, "Your IP has been banned by a moderator.");
    return;
  }

  m_connection_pool->create(l_socket, clientAddress);
}

theory::ServerInfo kenji::Server::serverInfo() const
{
  theory::ServerInfo l_info;
  l_info.name = ConfigManager::serverName();
  l_info.description = ConfigManager::serverDescription();
  l_info.softwareName = softwareName();
  l_info.softwareVersion = softwareVersion();
  l_info.protocolName = theory::protocolName();
  l_info.protocolVersion = theory::protocolVersion();
  l_info.playerCount = m_player_count;
  l_info.maxPlayers = ConfigManager::maxPlayers();
  l_info.badgeIds = _badgeEngine->badgeIds();
  return l_info;
}

theory::ServerSettings kenji::Server::serverSettings() const
{
  theory::ServerSettings l_settings;
  l_settings.name = ConfigManager::serverNickname();
  const QUrl l_asset_url = ConfigManager::assetUrl();
  if (l_asset_url.isValid())
  {
    l_settings.assetUrl = QString::fromUtf8(l_asset_url.toEncoded(QUrl::EncodeSpaces));
  }

  l_settings.messageOfTheDay = ConfigManager::motd();
  l_settings.maxNameLength = ConfigManager::maxNameLength();
  l_settings.maxMessageLength = ConfigManager::maxMessageLength();
  l_settings.maxCharacterNameLength = ConfigManager::maxCharacterNameLength();
  l_settings.maxIcMessageLength = ConfigManager::maxIcMessageLength();
  l_settings.maxEvidenceNameLength = ConfigManager::maxEvidenceNameLength();
  l_settings.maxEvidenceDescriptionLength = ConfigManager::maxEvidenceDescriptionLength();
  l_settings.maxInventorySize = ConfigManager::maxInventorySize();
  l_settings.maxPersonalInventorySize = ConfigManager::maxPersonalInventorySize();
  return l_settings;
}

bool kenji::Server::isMessageAllowed() const
{
  return m_can_send_ic_messages;
}

void kenji::Server::startMessageFloodguard(int f_duration)
{
  m_can_send_ic_messages = false;
  m_message_floodguard_timer->start(f_duration);
}

bool kenji::Server::personalInventoriesEnabled() const
{
  return m_personal_inventories_enabled;
}

void kenji::Server::setPersonalInventoriesEnabled(bool enabled)
{
  m_personal_inventories_enabled = enabled;
  if (!enabled)
  {
    for (const AOClient *l_client : m_client_registry->clients())
    {
      m_inventory_registry->clear(l_client->inventoryId);
    }
  }

  Q_EMIT personalInventoriesToggled(enabled);
}

QHostAddress kenji::Server::parseToIPv4(const QHostAddress &f_remote_ip)
{
  bool l_ok;
  QHostAddress l_remote_ip = f_remote_ip;
  QHostAddress l_temp_remote_ip = QHostAddress(f_remote_ip.toIPv4Address(&l_ok));
  if (l_ok)
  {
    l_remote_ip = l_temp_remote_ip;
  }

  return l_remote_ip;
}

void kenji::Server::reloadSettings()
{
  ConfigManager::reloadSettings();
  _host->setInfoRateLimit(ConfigManager::infoRateLimit(), ConfigManager::infoRateWindow());
  if (const auto error = _badgeEngine->setBadgeIds(ConfigManager::badgeIds()))
  {
    zWarning(log::main) << QStringLiteral("badges unchanged: %1").arg(error->toString());
  }

  Q_EMIT reloadRequest(ConfigManager::serverNickname(), ConfigManager::serverDescription());
  Q_EMIT updateHTTPConfiguration();
  handleDiscordIntegration();
  logger->loadLogtext();
  m_ipban_list = ConfigManager::iprangeBans();
  acl_roles_handler->loadFile("config/acl_roles.ini");
  command_extension_collection->loadFile("config/command_extensions.ini");
}

void kenji::Server::broadcast(const theory::Packet &packet)
{
  const QList<AOClient *> l_clients = m_client_registry->clients();
  for (AOClient *l_client : l_clients)
  {
    l_client->shipPacket(packet);
  }
}

void kenji::Server::broadcastIf(const theory::Packet &packet, const std::function<bool(const AOClient &)> &condition)
{
  const QList<AOClient *> l_clients = m_client_registry->clients();
  for (AOClient *l_client : l_clients)
  {
    if (condition(*l_client))
    {
      l_client->shipPacket(packet);
    }
  }
}

void kenji::Server::broadcastToArea(const theory::Packet &packet, theory::AreaId areaId)
{
  AreaData *l_area = m_areas.value(areaId);
  if (l_area == nullptr)
  {
    return;
  }

  const QList<theory::PlayerId> l_player_ids = l_area->joinedIDs();
  for (const theory::PlayerId l_player_id : l_player_ids)
  {
    getClientByID(l_player_id)->shipPacket(packet);
  }
}

void kenji::Server::broadcastToPlayer(const theory::Packet &packet, theory::PlayerId playerId)
{
  AOClient *l_client = getClientByID(playerId);
  if (l_client != nullptr)
  { // This should never happen, but safety first.
    l_client->shipPacket(packet);
    return;
  }
}

void kenji::Server::broadcastMessage(const QString &message, theory::ServerMessagePacket::Level level)
{
  theory::ServerMessagePacket l_packet;
  l_packet.message = message;
  l_packet.level = level;
  broadcast(l_packet);
}

void kenji::Server::broadcastMessageIf(const QString &message, const std::function<bool(const AOClient &)> &condition, theory::ServerMessagePacket::Level level)
{
  theory::ServerMessagePacket l_packet;
  l_packet.message = message;
  l_packet.level = level;
  broadcastIf(l_packet, condition);
}

void kenji::Server::broadcastMessageToArea(const QString &message, theory::AreaId areaId, theory::ServerMessagePacket::Level level)
{
  theory::ServerMessagePacket l_packet;
  l_packet.message = message;
  l_packet.level = level;
  broadcastToArea(l_packet, areaId);
}

void kenji::Server::broadcastMessageToPlayer(const QString &message, theory::PlayerId playerId, theory::ServerMessagePacket::Level level)
{
  theory::ServerMessagePacket l_packet;
  l_packet.message = message;
  l_packet.level = level;
  broadcastToPlayer(l_packet, playerId);
}

QList<kenji::AOClient *> kenji::Server::getClientsByUserId(theory::UserId userId)
{
  return m_client_registry->clientsByUserId(userId);
}

kenji::AOClient *kenji::Server::getClientByID(theory::PlayerId id)
{
  return m_client_registry->client(id);
}

void kenji::Server::wipeAllTokens()
{
  m_connection_pool->clear();
  m_session_registry->dropAll();
  _guests->clear();
  _users->invalidateAllTokens();
}

int kenji::Server::getPlayerCount()
{
  return m_player_count;
}

QList<theory::CharacterId> kenji::Server::getCharacters()
{
  return m_characters;
}

QList<kenji::AreaData *> kenji::Server::getAreas()
{
  return m_areas;
}

int kenji::Server::getAreaCount()
{
  return m_areas.length();
}

kenji::AreaData *kenji::Server::getAreaById(theory::AreaId f_area_id)
{
  AreaData *l_area = nullptr;

  if (f_area_id >= 0 && f_area_id < m_areas.length())
  {
    l_area = m_areas.at(f_area_id);
  }

  return l_area;
}

kenji::AreaData *kenji::Server::defaultArea() const
{
  return m_areas.first();
}

QQueue<QString> kenji::Server::getAreaBuffer(const QString &f_areaName)
{
  return logger->buffer(f_areaName);
}

QStringList kenji::Server::getAreaNames()
{
  return m_area_names;
}

QString kenji::Server::getAreaName(theory::AreaId f_area_id)
{
  QString l_name;

  if (f_area_id >= 0 && f_area_id < m_area_names.length())
  {
    l_name = m_area_names.at(f_area_id);
  }

  return l_name;
}

QStringList kenji::Server::getBackgrounds()
{
  return m_backgrounds;
}

kenji::ServerDatabase &kenji::Server::database()
{
  return *_database;
}

kenji::MedievalParser *kenji::Server::getMedievalParser()
{
  return medieval_parser.get();
}

kenji::ACLRolesHandler *kenji::Server::getACLRolesHandler()
{
  return acl_roles_handler;
}

kenji::CommandExtensionCollection *kenji::Server::getCommandExtensionCollection()
{
  return command_extension_collection;
}

kenji::ClientGameObserver &kenji::Server::gameObserver(theory::PlayerId playerId) const
{
  return *m_game_observers.value(playerId);
}

void kenji::Server::allowMessage()
{
  m_can_send_ic_messages = true;
}

void kenji::Server::synchronize()
{
  if (m_global_timer->state() == theory::TimerState::Running)
  {
    broadcast(makeTimerPacket(*m_global_timer, theory::TimerPacket::Tick));
  }

  for (AreaData *l_area : qAsConst(m_areas))
  {
    l_area->synchronize();
  }
}

kenji::Timer *kenji::Server::globalTimer() const
{
  return m_global_timer;
}

void kenji::Server::shipGlobalTimer(theory::PlayerId f_player_id)
{
  broadcastToPlayer(makeTimerPacket(*m_global_timer, theory::TimerPacket::Visibility), f_player_id);
  broadcastToPlayer(makeTimerPacket(*m_global_timer, theory::TimerPacket::State), f_player_id);
  broadcastToPlayer(makeTimerPacket(*m_global_timer, theory::TimerPacket::Tick), f_player_id);
}

void kenji::Server::handleDiscordIntegration()
{
  // Prevent double connecting by preemtively disconnecting them.
  this->disconnect(discord.get());

  if (ConfigManager::discordWebhookEnabled())
  {
    if (ConfigManager::discordModcallWebhookEnabled())
    {
      connect(this, &Server::modcallWebhookRequest, discord.get(), &Discord::onModcallWebhookRequested);
    }

    if (ConfigManager::discordBanWebhookEnabled())
    {
      connect(this, &Server::banWebhookRequest, discord.get(), &Discord::onBanWebhookRequested);
    }
  }

  return;
}

void kenji::Server::increasePlayerCount()
{
  m_player_count++;
}

void kenji::Server::decreasePlayerCount()
{
  m_player_count--;
}

bool kenji::Server::isIPBanned(const QHostAddress &f_remote_IP)
{
  bool l_match_found = false;
  for (const QString &l_ipban : qAsConst(m_ipban_list))
  {
    if (f_remote_IP.isInSubnet(QHostAddress::parseSubnet(l_ipban)))
    {
      l_match_found = true;
      break;
    }
  }

  return l_match_found;
}

kenji::Server::~Server()
{
  m_sync_timer->stop();
}
