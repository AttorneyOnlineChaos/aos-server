#pragma once

#include "acl_roles_handler.h"
#include "ao_client.h"
#include "ao_client_registry.h"
#include "area_data.h"
#include "broadcaster.h"
#include "command_extension.h"
#include "config_manager.h"
#include "connection_pool.h"
#include "core/pointer_types.h"
#include "db_manager.h"
#include "discord.h"
#include "join_floodguard.h"
#include "logger/u_logger.h"
#include "medieval_parser.h"
#include "music_manager.h"
#include "network/packet.h"
#include "network/packet_factory.h"
#include "player_state_observer.h"
#include "server_publisher.h"
#include "session_registry.h"
#include "timer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QMap>
#include <QSettings>
#include <QStack>
#include <QString>
#include <QTimer>
#include <QWebSocket>

#include <functional>

namespace kenji
{
/**
 * @brief The class that represents the actual server as it is.
 */
class Server : public QObject, public Broadcaster
{
  Q_OBJECT

public:
  /**
   * @brief Creates a Server instance.
   *
   * @param p_ws_port The port to listen for connections on.
   * @param f_packet_factory The packet factory duh.
   * @param parent Qt-based parent, passed along to inherited constructor from QObject.
   */
  Server(int p_ws_port, const theory::PacketFactory &f_packet_factory, QObject *parent = nullptr);

  /**
   * @brief Destructor for the Server class.
   *
   * @details Marks every Client, the WSProxy, the underlying #server, and the database manager to be deleted later.
   */
  ~Server();

  /**
   * @brief Starts the server.
   *
   * @details Starts listening for incoming connections on the given port.
   *
   * Advertising is not done here -- see Advertiser::contactMasterServer() for that.
   *
   * @return True if the server is listening, false if the port could not be bound.
   */
  bool start();

  /**
   * @brief Returns a list of all clients currently in the server.
   *
   * @return A list of all clients currently in the server.
   */
  QList<AOClient *> getClients();

  /**
   * @brief Gets a list of pointers to all clients with the given IPID.
   *
   * @param ipid The IPID to look for.
   *
   * @return A list of clients whose IPID match. List may be empty.
   */
  QList<AOClient *> getClientsByIpid(const QString &ipid);

  /**
   * @brief Gets a list of pointers to all clients with the given HWID.
   *
   * @param HWID The HWID to look for.
   *
   * @return A list of clients whose HWID match. List may be empty.
   */
  QList<AOClient *> getClientsByHwid(const QString &f_hwid);

  /**
   * @brief Gets a pointer to a client by user ID.
   *
   * @param id The user ID to look for.
   *
   * @return A pointer to the client if found, a nullpointer if not.
   */
  AOClient *getClientByID(theory::PlayerId id);

  /**
   * @brief Returns the overall player count in the server.
   *
   * @return The overall player count in the server.
   */
  int getPlayerCount();

  /**
   * @brief Returns a list of the available characters on the server to use.
   *
   * @return A list of the available characters on the server to use.
   */
  QList<theory::CharacterId> getCharacters();

  /**
   * @brief Sends a packet to all clients in the server.
   *
   * @param packet The packet to send to the clients.
   */
  void broadcast(const theory::Packet &packet) override;

  void broadcastIf(const theory::Packet &packet, const std::function<bool(const AOClient &)> &condition) override;

  /**
   * @brief Sends a packet to all clients in a given area.
   *
   * @param packet The packet to send to the clients.
   *
   * @param areaId The index of the area to look for clients in.
   *
   * @note Does nothing if an area by the given index does not exist.
   */
  void broadcastToArea(const theory::Packet &packet, theory::AreaId areaId) override;

  /**
   * @brief Sends a packet to a single client.
   *
   * @param The packet send to the client.
   *
   * @param The temporary userID of the client.
   */
  void broadcastToPlayer(const theory::Packet &packet, theory::PlayerId playerId) override;

  void broadcastMessage(const QString &message, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) override;

  void broadcastMessageIf(const QString &message, const std::function<bool(const AOClient &)> &condition, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) override;

  void broadcastMessageToArea(const QString &message, theory::AreaId areaId, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) override;

  void broadcastMessageToPlayer(const QString &message, theory::PlayerId playerId, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) override;

  /**
   * @brief Returns the server-wide timer.
   */
  Timer *globalTimer() const;

  /**
   * @brief Sends the state of the global timer.
   */
  void shipGlobalTimer(theory::PlayerId f_player_id);

  /**
   * @brief Checks if an IP is in a subnet of the IPBanlist.
   **/
  bool isIPBanned(const QHostAddress &f_remote_IP);

  /**
   * @brief Returns the list of areas in the server.
   *
   * @return A list of areas.
   */
  QList<AreaData *> getAreas();

  /**
   * @brief Returns the number of areas in the server.
   */
  int getAreaCount();

  /**
   * @brief Returns a pointer to the area associated with the index.
   *
   * @param f_area_id The index of the area.
   *
   * @return A pointer to the area or null.
   */
  AreaData *getAreaById(theory::AreaId f_area_id);

  /**
   * @brief Getter for an area specific buffer from the logger.
   */
  QQueue<QString> getAreaBuffer(const QString &f_areaName);

  /**
   * @brief The names of the areas on the server.
   *
   * @return A list of names.
   */
  QStringList getAreaNames();

  /**
   * @brief Returns the name of the area associated with the index.
   *
   * @param f_area_id The index of the area.
   *
   * @return The name of the area or empty.
   */
  QString getAreaName(theory::AreaId f_area_id);

  /**
   * @brief Returns the available backgrounds on the server.
   *
   * @return A list of backgrounds.
   */
  QStringList getBackgrounds();

  /**
   * @brief Returns a pointer to a database manager.
   *
   * @return A pointer to a database manager.
   */
  DBManager *getDatabaseManager();

  /**
   * @brief Returns a pointer to the server's Ye Olde Chat Filter
   */
  MedievalParser *getMedievalParser();

  /**
   * @brief Returns a pointer to ACL role handler.
   */
  ACLRolesHandler *getACLRolesHandler();

  /**
   * @brief Returns a pointer to a command extension collection.
   */
  CommandExtensionCollection *getCommandExtensionCollection();

  /**
   * @brief Returns whatever a game message may be broadcasted or not.
   *
   * @return True if expired; false otherwise.
   */
  bool isMessageAllowed() const;

  /**
   * @brief Starts a global timer that determines whatever a game message may be broadcasted or not.
   *
   * @param f_duration The duration of the message floodguard timer.
   */
  void startMessageFloodguard(int f_duration);

  /**
   * @brief Attempts to parse a IPv6 mapped IPv4 to an IPv4.
   */
  QHostAddress parseToIPv4(const QHostAddress &f_remote_ip);

public Q_SLOTS:
  /**
   * @brief Convenience class to call a reload of available configuraiton elements.
   */
  void reloadSettings();

  /**
   * @brief Handles a new connection.
   *
   * @details The function creates an AOClient to represent the user, assigns a user ID to them, and
   * checks if the client is banned.
   */
  void processPendingConnection();

  /**
   * @brief Method to construct and reconstruct Discord Webhook Integration.
   *
   * @details Constructs or rebuilds Discord Object during server startup and configuration reload.
   */
  void handleDiscordIntegration();

Q_SIGNALS:

  /**
   * @brief Sends the server name and description, emitted by /reload.
   *
   * @param p_name The server name.
   * @param p_desc The server description.
   */
  void reloadRequest(const QString &p_name, const QString &p_desc);

  /**
   * @brief Triggers a partial update of the modern advertiser as some information, such as ports
   * can't be updated while the server is running.
   */
  void updateHTTPConfiguration();

  /**
   * @brief Sends a modcall webhook request, emitted by AOClient::pktModcall.
   *
   * @param f_name The character or OOC name of the client who sent the modcall.
   * @param f_area The name of the area the modcall was sent from.
   * @param f_reason The reason the client specified for the modcall.
   * @param f_id The client id of the client who sent the modcall.
   * @param f_buffer The area's log buffer.
   */
  void modcallWebhookRequest(const QString &f_name, const QString &f_area, const QString &f_id, const QString &f_reason, const QQueue<QString> &f_buffer);

  /**
   * @brief Sends a ban webhook request, emitted by AOClient::cmdBan
   * @param f_ipid The IPID of the banned client.
   * @param f_moderator The moderator who issued the ban.
   * @param f_duration The duration of the ban in a human readable format.
   * @param f_reason The reason for the ban.
   * @param f_banID The ID of the issued ban.
   */
  void banWebhookRequest(const QString &f_ipid, const QString &f_moderator, const QString &f_duration, const QString &f_reason, const int &f_banID);

private:
  /**
   * @brief Listens for incoming connections.
   */
  QHttpServer *m_http = nullptr;

  JoinFloodguard m_join_floodguard;

  /**
   * @brief Handles Discord webhooks.
   */
  theory::Unique<Discord> discord;

  /**
   * @brief Handles HTTP server advertising.
   */
  theory::Unique<ServerPublisher> server_publisher;

  /**
   * @brief Handles the universal log framework.
   */
  ULogger *logger;

  /**
   * @brief Handles all musiclists.
   */
  MusicManager *music_manager;

  /**
   * @brief The port through which the server will accept WebSocket connections.
   */
  int m_port;

  const theory::PacketFactory &m_packet_factory;

  /**
   * @brief Medieval mode text parser class
   */
  theory::Unique<MedievalParser> medieval_parser;

  theory::Unique<AOClientRegistry> m_client_registry;

  theory::Unique<SessionRegistry> m_session_registry;

  theory::Unique<ConnectionPool> m_connection_pool;

  PlayerStateObserver m_player_state_observer;

  /**
   * @brief The overall player count in the server.
   */
  int m_player_count;

  /**
   * @brief The characters available on the server to use.
   */
  QList<theory::CharacterId> m_characters;

  /**
   * @brief The areas on the server.
   */
  QList<AreaData *> m_areas;

  /**
   * @brief The names of the areas on the server.
   *
   * @details Equivalent to iterating over #areas and getting the area names individually, but grouped together
   * here for faster access.
   */
  QStringList m_area_names;

  /**
   * @brief The backgrounds on the server that may be used in areas.
   */
  QStringList m_backgrounds;

  /**
   * @brief Collection of all IPs that are banned.
   */
  QStringList m_ipban_list;

  /**
   * @brief Timer until the next IC message can be sent.
   */
  QTimer *m_message_floodguard_timer;

  /**
   * @brief If false, IC messages will be rejected.
   */
  bool m_can_send_ic_messages = true;

  /**
   * @brief The database manager on the server, used to store users' bans and authorisation details.
   */
  DBManager *db_manager;

  /**
   * @see ACLRolesHandler
   */
  ACLRolesHandler *acl_roles_handler;

  /**
   * @see CommandExtensionCollection
   */
  CommandExtensionCollection *command_extension_collection;

  QHttpServerResponse serverInfoResponse(const QHttpServerRequest &request);

  QTimer *m_sync_timer;

  Timer *m_global_timer;

private Q_SLOTS:
  void synchronize();

  /**
   * @brief Increase the current player count by one.
   */
  void increasePlayerCount();

  /**
   * @brief Decrease the current player count based on the client id provided.
   *
   * @param f_client_id The client id of the client to check.
   */
  void decreasePlayerCount();

  /**
   * @brief Allow game messages to be broadcasted.
   */
  void allowMessage();
};
} // namespace kenji
