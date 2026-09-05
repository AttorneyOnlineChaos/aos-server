#include "server.h"

#include "acl_roles_handler.h"
#include "ao_client.h"
#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "core/json_codec.h"
#include "core/logging.h"
#include "db_manager.h"
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
#include "server_publisher.h"

#include <QCryptographicHash>
#include <QHttpServerWebSocketUpgradeResponse>
#include <QTcpServer>

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

  db_manager = new DBManager(this);
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

  m_http = new QHttpServer(this);
  m_http->route("/info", [this](const QHttpServerRequest &request) { return serverInfoResponse(request); });
  m_http->addWebSocketUpgradeVerifier(this, [](const QHttpServerRequest &request) {
    if (request.url().path() == "/join")
    {
      return QHttpServerWebSocketUpgradeResponse::accept();
    }
    return QHttpServerWebSocketUpgradeResponse::passToNext();
  });
  connect(m_http, &QHttpServer::newWebSocketConnection, this, &Server::processPendingConnection);

  quint16 l_port = 0;
  bool l_listening = false;
  QTcpServer *l_tcp = new QTcpServer;
  if (!l_tcp->listen(bind_addr, m_port) || !m_http->bind(l_tcp))
  {
    zDebug(log::network) << "Server error:" << l_tcp->errorString();
    delete l_tcp;
  }
  else
  {
    l_port = l_tcp->serverPort();
    l_listening = true;
    zInfo(log::network) << "Server listening on" << l_port;
  }

  // Checks if any Discord webhooks are enabled.
  handleDiscordIntegration();

  // Construct modern advertiser if enabled in config
  server_publisher = theory::makeUnique<ServerPublisher>(l_port, &m_player_count);
  connect(this, &Server::updateHTTPConfiguration, server_publisher.get(), &ServerPublisher::publishServer);

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
  m_session_registry = theory::makeUnique<SessionRegistry>(*m_client_registry);
  m_connection_pool = theory::makeUnique<ConnectionPool>(*m_session_registry, *db_manager);
  connect(m_connection_pool.get(), &ConnectionPool::connectionAttempted, logger, &ULogger::logConnectionAttempt);

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

  return l_listening;
}

QList<kenji::AOClient *> kenji::Server::getClients()
{
  return m_client_registry->clients();
}

void kenji::Server::processPendingConnection()
{
  while (m_http->hasPendingWebSocketConnections())
  {
    QWebSocket *socket = m_http->nextPendingWebSocketConnection().release();

    // TLDR : We check if the header comes trough a proxy/tunnel running locally.
    // This is to ensure nobody can send those headers from the web.
    bool l_is_local = (socket->peerAddress() == QHostAddress::LocalHost) || (socket->peerAddress() == QHostAddress::LocalHostIPv6) || (socket->peerAddress() == QHostAddress("::ffff:127.0.0.1"));
    QNetworkRequest l_request = socket->request();
    QHostAddress l_client_ip;
    if (l_request.hasRawHeader("x-real-ip") && l_is_local)
    {
      l_client_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-real-ip")));
    }
    else if (l_request.hasRawHeader("x-forwarded-for") && l_is_local)
    {
      l_client_ip = QHostAddress(QString::fromUtf8(l_request.rawHeader("x-forwarded-for")));
    }
    else
    {
      l_client_ip = socket->peerAddress();
    }

    socket->setMaxAllowedIncomingMessageSize(ConfigManager::maxPacketSize());

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
      continue;
    }

    QString l_ipid = QCryptographicHash::hash(l_client_ip.toString().toUtf8(), QCryptographicHash::Md5).toHex().right(8);

    auto ban = db_manager->isIPBanned(l_ipid);
    bool is_banned = ban.first;
    int multiclient_count = 1 + m_client_registry->countByAddress(l_client_ip);
    bool is_at_multiclient_limit = multiclient_count > ConfigManager::multiClientLimit() && !l_client_ip.isLoopback();

    if (is_banned)
    {
      refuse(theory::ErrorPacket::Banned, "Reason: " + ban.second.reason + "\nBan ID: " + QString::number(ban.second.id) + "\nUntil: " + ban.second.until());
      continue;
    }

    if (is_at_multiclient_limit)
    {
      refuse(theory::ErrorPacket::ServerFull);
      continue;
    }

    QHostAddress l_remote_ip = l_client_ip;
    if (l_remote_ip.protocol() == QAbstractSocket::IPv6Protocol)
    {
      l_remote_ip = parseToIPv4(l_remote_ip);
    }

    if (isIPBanned(l_remote_ip))
    {
      refuse(theory::ErrorPacket::Banned, "Your IP has been banned by a moderator.");
      continue;
    }

    m_connection_pool->create(l_socket, l_client_ip, l_ipid);
  }
}

QHttpServerResponse kenji::Server::serverInfoResponse(const QHttpServerRequest &request)
{
  QHostAddress l_address = request.remoteAddress();
  bool l_is_local = (l_address == QHostAddress::LocalHost) || (l_address == QHostAddress::LocalHostIPv6) || (l_address == QHostAddress("::ffff:127.0.0.1"));
  if (l_is_local)
  {
    const QByteArray l_real_ip = request.value("x-real-ip");
    const QByteArray l_forwarded_for = request.value("x-forwarded-for");
    if (!l_real_ip.isEmpty())
    {
      l_address = QHostAddress(QString::fromUtf8(l_real_ip));
    }
    else if (!l_forwarded_for.isEmpty())
    {
      l_address = QHostAddress(QString::fromUtf8(l_forwarded_for));
    }
  }

  m_join_floodguard.setLimit(ConfigManager::infoRateLimit(), 3600000);
  if (!m_join_floodguard.allow(l_address.toString()))
  {
    return QHttpServerResponse(QHttpServerResponder::StatusCode::TooManyRequests);
  }

  theory::ServerInfo l_info;
  l_info.name = ConfigManager::serverNickname();
  l_info.description = ConfigManager::serverDescription();
  l_info.softwareName = softwareName();
  l_info.softwareVersion = softwareVersion();
  l_info.protocolName = theory::protocolName();
  l_info.protocolVersion = theory::protocolVersion();
  l_info.playerCount = m_player_count;
  l_info.maxPlayers = ConfigManager::maxPlayers();

  return QHttpServerResponse(theory::encodeJson(l_info).toObject());
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

QList<kenji::AOClient *> kenji::Server::getClientsByIpid(const QString &ipid)
{
  return m_client_registry->clientsByIpid(ipid);
}

QList<kenji::AOClient *> kenji::Server::getClientsByHwid(const QString &f_hwid)
{
  return m_client_registry->clientsByHwid(f_hwid);
}

kenji::AOClient *kenji::Server::getClientByID(theory::PlayerId id)
{
  return m_client_registry->client(id);
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

kenji::DBManager *kenji::Server::getDatabaseManager()
{
  return db_manager;
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
