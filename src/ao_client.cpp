#include "ao_client.h"

#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "core/json_codec.h"
#include "core/logging.h"
#include "kenji_defs.h"
#include "server.h"

const QMap<QString, kenji::AOClient::CommandInfo> kenji::AOClient::COMMANDS{
    {"login", {{ACLRole::NONE}, 0, &AOClient::cmdLogin}},
    {"getarea", {{ACLRole::NONE}, 0, &AOClient::cmdGetArea}},
    {"getareas", {{ACLRole::NONE}, 0, &AOClient::cmdGetAreas}},
    {"ban", {{ACLRole::BAN}, 3, &AOClient::cmdBan}},
    {"kick", {{ACLRole::KICK}, 2, &AOClient::cmdKick}},
    {"changeauth", {{ACLRole::SUPER}, 0, &AOClient::cmdChangeAuth}},
    {"rootpass", {{ACLRole::SUPER}, 1, &AOClient::cmdSetRootPass}},
    {"background", {{ACLRole::NONE}, 1, &AOClient::cmdSetBackground}},
    {"side", {{ACLRole::CM}, 0, &AOClient::cmdSetSide}},
    {"lock_background", {{ACLRole::CM, ACLRole::BGLOCK}, 0, &AOClient::cmdBgLock}},
    {"unlock_background", {{ACLRole::CM, ACLRole::BGLOCK}, 0, &AOClient::cmdBgUnlock}},
    {"adduser", {{ACLRole::MODIFY_USERS}, 2, &AOClient::cmdAddUser}},
    {"removeuser", {{ACLRole::MODIFY_USERS}, 1, &AOClient::cmdRemoveUser}},
    {"listusers", {{ACLRole::MODIFY_USERS}, 0, &AOClient::cmdListUsers}},
    {"setperms", {{ACLRole::MODIFY_USERS}, 2, &AOClient::cmdSetPerms}},
    {"removeperms", {{ACLRole::MODIFY_USERS}, 1, &AOClient::cmdRemovePerms}},
    {"listperms", {{ACLRole::NONE}, 0, &AOClient::cmdListPerms}},
    {"logout", {{ACLRole::NONE}, 0, &AOClient::cmdLogout}},
    {"pos", {{ACLRole::NONE}, 1, &AOClient::cmdPos}},
    {"g", {{ACLRole::NONE}, 1, &AOClient::cmdG}},
    {"need", {{ACLRole::NONE}, 1, &AOClient::cmdNeed}},
    {"coinflip", {{ACLRole::NONE}, 0, &AOClient::cmdFlip}},
    {"roll", {{ACLRole::NONE}, 0, &AOClient::cmdRoll}},
    {"rolla", {{ACLRole::NONE}, 0, &AOClient::cmdRollA}},
    {"rollp", {{ACLRole::NONE}, 0, &AOClient::cmdRollP}},
    {"doc", {{ACLRole::NONE}, 0, &AOClient::cmdDoc}},
    {"cleardoc", {{ACLRole::NONE}, 0, &AOClient::cmdClearDoc}},
    {"cm", {{ACLRole::NONE}, 0, &AOClient::cmdCM}},
    {"uncm", {{ACLRole::CM}, 0, &AOClient::cmdUnCM}},
    {"invite", {{ACLRole::CM}, 1, &AOClient::cmdInvite}},
    {"uninvite", {{ACLRole::CM}, 1, &AOClient::cmdUnInvite}},
    {"area_lock", {{ACLRole::CM}, 0, &AOClient::cmdLock}},
    {"area_spectate", {{ACLRole::CM}, 0, &AOClient::cmdSpectatable}},
    {"area_unlock", {{ACLRole::CM}, 0, &AOClient::cmdUnLock}},
    {"timer", {{ACLRole::CM}, 0, &AOClient::cmdTimer}},
    {"area", {{ACLRole::NONE}, 1, &AOClient::cmdArea}},
    {"play", {{ACLRole::NONE}, 1, &AOClient::cmdPlay}},
    {"area_kick", {{ACLRole::CM}, 1, &AOClient::cmdAreaKick}},
    {"randomchar", {{ACLRole::NONE}, 0, &AOClient::cmdRandomChar}},
    {"switch", {{ACLRole::NONE}, 1, &AOClient::cmdSwitch}},
    {"toggleglobal", {{ACLRole::NONE}, 0, &AOClient::cmdToggleGlobal}},
    {"mods", {{ACLRole::NONE}, 0, &AOClient::cmdMods}},
    {"commands", {{ACLRole::NONE}, 0, &AOClient::cmdCommands}},
    {"status", {{ACLRole::NONE}, 1, &AOClient::cmdStatus}},
    {"forcepos", {{ACLRole::CM}, 2, &AOClient::cmdForcePos}},
    {"currentmusic", {{ACLRole::NONE}, 0, &AOClient::cmdCurrentMusic}},
    {"pm", {{ACLRole::NONE}, 2, &AOClient::cmdPM}},
    {"motd", {{ACLRole::NONE}, 0, &AOClient::cmdMOTD}},
    {"set_motd", {{ACLRole::MOTD}, 1, &AOClient::cmdSetMOTD}},
    {"announce", {{ACLRole::ANNOUNCE}, 1, &AOClient::cmdAnnounce}},
    {"m", {{ACLRole::MODCHAT}, 1, &AOClient::cmdM}},
    {"gm", {{ACLRole::MODCHAT}, 1, &AOClient::cmdGM}},
    {"mute", {{ACLRole::MUTE}, 1, &AOClient::cmdMute}},
    {"unmute", {{ACLRole::MUTE}, 1, &AOClient::cmdUnMute}},
    {"bans", {{ACLRole::BAN}, 0, &AOClient::cmdBans}},
    {"unban", {{ACLRole::BAN}, 1, &AOClient::cmdUnBan}},
    {"subtheme", {{ACLRole::CM}, 1, &AOClient::cmdSubTheme}},
    {"notecard", {{ACLRole::NONE}, 1, &AOClient::cmdNoteCard}},
    {"notecard_reveal", {{ACLRole::CM}, 0, &AOClient::cmdNoteCardReveal}},
    {"notecard_clear", {{ACLRole::NONE}, 0, &AOClient::cmdNoteCardClear}},
    {"8ball", {{ACLRole::NONE}, 1, &AOClient::cmd8Ball}},
    {"lm", {{ACLRole::MODCHAT}, 1, &AOClient::cmdLM}},
    {"judgelog", {{ACLRole::CM}, 0, &AOClient::cmdJudgeLog}},
    {"allow_blankposting", {{ACLRole::MODCHAT}, 0, &AOClient::cmdAllowBlankposting}},
    {"toggle_inventory", {{ACLRole::MODCHAT}, 0, &AOClient::cmdToggleInventory}},
    {"gimp", {{ACLRole::MUTE}, 1, &AOClient::cmdGimp}},
    {"ungimp", {{ACLRole::MUTE}, 1, &AOClient::cmdUnGimp}},
    {"baninfo", {{ACLRole::BAN}, 1, &AOClient::cmdBanInfo}},
    {"testify", {{ACLRole::CM}, 0, &AOClient::cmdTestify}},
    {"testimony", {{ACLRole::NONE}, 0, &AOClient::cmdTestimony}},
    {"examine", {{ACLRole::CM}, 0, &AOClient::cmdExamine}},
    {"pause", {{ACLRole::CM}, 0, &AOClient::cmdPauseTestimony}},
    {"delete", {{ACLRole::CM}, 0, &AOClient::cmdDeleteStatement}},
    {"update", {{ACLRole::CM}, 0, &AOClient::cmdUpdateStatement}},
    {"add", {{ACLRole::CM}, 0, &AOClient::cmdAddStatement}},
    {"reload", {{ACLRole::SUPER}, 0, &AOClient::cmdReload}},
    {"disemvowel", {{ACLRole::MUTE}, 1, &AOClient::cmdDisemvowel}},
    {"undisemvowel", {{ACLRole::MUTE}, 1, &AOClient::cmdUnDisemvowel}},
    {"shake", {{ACLRole::MUTE}, 1, &AOClient::cmdShake}},
    {"unshake", {{ACLRole::MUTE}, 1, &AOClient::cmdUnShake}},
    {"force_noint_pres", {{ACLRole::CM}, 0, &AOClient::cmdForceImmediate}},
    {"allow_iniswap", {{ACLRole::CM}, 0, &AOClient::cmdAllowIniswap}},
    {"afk", {{ACLRole::NONE}, 0, &AOClient::cmdAfk}},
    {"savetestimony", {{ACLRole::NONE}, 1, &AOClient::cmdSaveTestimony}},
    {"loadtestimony", {{ACLRole::CM}, 1, &AOClient::cmdLoadTestimony}},
    {"permitsaving", {{ACLRole::MODCHAT}, 1, &AOClient::cmdPermitSaving}},
    {"mutepm", {{ACLRole::NONE}, 0, &AOClient::cmdMutePM}},
    {"toggleadverts", {{ACLRole::NONE}, 0, &AOClient::cmdToggleAdverts}},
    {"ooc_mute", {{ACLRole::MUTE}, 1, &AOClient::cmdOocMute}},
    {"ooc_unmute", {{ACLRole::MUTE}, 1, &AOClient::cmdOocUnMute}},
    {"block_wtce", {{ACLRole::MUTE}, 1, &AOClient::cmdBlockWtce}},
    {"unblock_wtce", {{ACLRole::MUTE}, 1, &AOClient::cmdUnBlockWtce}},
    {"block_dj", {{ACLRole::MUTE}, 1, &AOClient::cmdBlockDj}},
    {"unblock_dj", {{ACLRole::MUTE}, 1, &AOClient::cmdUnBlockDj}},
    {"charcurse", {{ACLRole::MUTE}, 1, &AOClient::cmdCharCurse}},
    {"uncharcurse", {{ACLRole::MUTE}, 1, &AOClient::cmdUnCharCurse}},
    {"charselect", {{ACLRole::NONE}, 0, &AOClient::cmdCharSelect}},
    {"force_charselect", {{ACLRole::FORCE_CHARSELECT}, 1, &AOClient::cmdForceCharSelect}},
    {"togglemusic", {{ACLRole::CM}, 0, &AOClient::cmdToggleMusic}},
    {"a", {{ACLRole::NONE}, 2, &AOClient::cmdA}},
    {"s", {{ACLRole::NONE}, 0, &AOClient::cmdS}},
    {"kick_uid", {{ACLRole::KICK}, 2, &AOClient::cmdKickUid}},
    {"firstperson", {{ACLRole::NONE}, 0, &AOClient::cmdFirstPerson}},
    {"update_ban", {{ACLRole::BAN}, 3, &AOClient::cmdUpdateBan}},
    {"changepass", {{ACLRole::NONE}, 1, &AOClient::cmdChangePassword}},
    {"ignore_bglist", {{ACLRole::IGNORE_BGLIST}, 0, &AOClient::cmdIgnoreBgList}},
    {"notice", {{ACLRole::SEND_NOTICE}, 1, &AOClient::cmdNotice}},
    {"noticeg", {{ACLRole::SEND_NOTICE}, 1, &AOClient::cmdNoticeGlobal}},
    {"togglejukebox", {{ACLRole::CM, ACLRole::JUKEBOX}, 0, &AOClient::cmdToggleJukebox}},
    {"help", {{ACLRole::NONE}, 0, &AOClient::cmdHelp}},
    {"togglemessage", {{ACLRole::CM}, 0, &AOClient::cmdToggleAreaMessageOnJoin}},
    {"clearmessage", {{ACLRole::CM}, 0, &AOClient::cmdClearAreaMessage}},
    {"areamessage", {{ACLRole::CM}, 0, &AOClient::cmdAreaMessage}},
    {"webfiles", {{ACLRole::NONE}, 0, &AOClient::cmdWebfiles}},
    {"addmusic", {{ACLRole::CM}, 1, &AOClient::cmdAddMusic}},
    {"addmusiccategory", {{ACLRole::CM}, 1, &AOClient::cmdAddMusicCategory}},
    {"removecustommusic", {{ACLRole::CM}, 1, &AOClient::cmdRemoveCustomMusic}},
    {"togglecustommusic", {{ACLRole::CM}, 0, &AOClient::cmdToggleCustomMusic}},
    {"clearcustommusic", {{ACLRole::CM}, 0, &AOClient::cmdClearCustomMusic}},
    {"toggle_wtce", {{ACLRole::CM}, 0, &AOClient::cmdToggleWtce}},
    {"toggle_shouts", {{ACLRole::CM}, 0, &AOClient::cmdToggleShouts}},
    {"kick_other", {{ACLRole::NONE}, 0, &AOClient::cmdKickOther}},
    {"dc", {{ACLRole::NONE}, 0, &AOClient::cmdDc}},
    {"jukebox_skip", {{ACLRole::CM}, 0, &AOClient::cmdJukeboxSkip}},
    {"play_ambience", {{ACLRole::NONE}, 1, &AOClient::cmdPlayAmbience}},
    {"medieval", {{ACLRole::MUTE}, 1, &AOClient::cmdMedieval}},
    {"unmedieval", {{ACLRole::MUTE}, 1, &AOClient::cmdUnMedieval}},
    {"medievalmode", {{ACLRole::MUTE}, 0, &AOClient::cmdMedievalMode}},
};

kenji::AOClient::SessionStatus kenji::AOClient::sessionStatus() const
{
  return m_session_status;
}

void kenji::AOClient::setSessionStatus(SessionStatus f_status)
{
  if (f_status != m_session_status)
  {
    m_session_status = f_status;
    Q_EMIT sessionStatusChanged(m_session_status);
  }
}

void kenji::AOClient::markActive()
{
  if (m_session_status == SessionStatus::Expired)
  {
    return;
  }
  m_session_timer->stop();
  setSessionStatus(SessionStatus::Active);
}

void kenji::AOClient::markInactive()
{
  if (m_session_status != SessionStatus::Active)
  {
    return;
  }

  const int l_timeout = ConfigManager::sessionTimeout();
  if (l_timeout <= 0)
  {
    markExpired();
    return;
  }
  m_session_timer->start(l_timeout * 1000);
  setSessionStatus(SessionStatus::Inactive);
}

void kenji::AOClient::markExpired()
{
  if (m_session_status == SessionStatus::Expired)
  {
    return;
  }
  m_session_timer->stop();
  setSessionStatus(SessionStatus::Expired);

  server->getAreaById(areaId())->removeClient(m_character, id);

  const QList<AreaData *> l_areas = server->getAreas();
  for (AreaData *l_area : l_areas)
  {
    if (l_area->invited().contains(id))
    {
      l_area->uninvite(id);
    }

    l_area->removeOwner(id);
  }
}

bool kenji::AOClient::processPendingPacket(const theory::Packet &packet)
{
#ifdef NET_DEBUG
  zDebug(log::protocol) << "Received packet:" << packet.header();
#endif

  qint64 current_tick = QDateTime::currentSecsSinceEpoch();
  if (rate_limit_tick < current_tick)
  {
    rate_limit_tick = current_tick;
    packet_count = 0;
  }

  ++packet_count;
  int hard_limit = ConfigManager::packetRateLimitHard();
  int soft_limit = ConfigManager::packetRateLimitSoft();

  if (hard_limit > 0 && packet_count >= hard_limit)
  {
    drop(theory::ErrorPacket::ProtocolError, "You have been disconnected for sending messages too quickly.");
    return false;
  }
  else if (soft_limit > 0 && packet_count >= soft_limit)
  {
    sendServerMessage("You are sending messages too quickly. Please slow down.");
  }

  if (m_status == theory::PlayerStatus::Away)
  {
    sendServerMessage("You are no longer AFK.");
    setStatus(theory::PlayerStatus::Online);
  }
  m_afk_timer->start(ConfigManager::afkTimeout() * 1000);

  if (!m_router.route(packet))
  {
    drop(theory::ErrorPacket::ProtocolError, "Invalid packet.");
    return false;
  }
  return m_session_status != SessionStatus::Expired;
}

void kenji::AOClient::drop()
{
  markExpired();
  m_socket->close();
}

void kenji::AOClient::drop(theory::ErrorPacket::Code code, const QString &reason)
{
  theory::ErrorPacket l_error;
  l_error.code = code;
  l_error.what = reason;
  shipPacket(l_error);
  drop();
}

void kenji::AOClient::registerSessionRoutes()
{
  m_router.unregisterAllRoutes();
  m_router.registerRoute<theory::GoodbyePacket>(&AOClient::process, this);
  m_router.registerRoute<theory::ChangeCharacterPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::OocMessagePacket>(&AOClient::process, this);
  m_router.registerRoute<theory::IcMessagePacket>(&AOClient::process, this);
  m_router.registerRoute<theory::PlayMusicPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::ChangeAreaPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::PenaltyPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::SplashPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::InventoryTransferPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::EvidenceRecordPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::EvidenceUpdatePacket>(&AOClient::process, this);
  m_router.registerRoute<theory::ModCallPacket>(&AOClient::process, this);
  m_router.registerRoute<theory::ModActionPacket>(&AOClient::process, this);
}

void kenji::AOClient::changeArea(theory::AreaId new_area)
{
  if (areaId() == new_area)
  {
    sendServerMessage("You are already in area " + server->getAreaName(areaId()));
    return;
  }
  if (server->getAreaById(new_area)->lockStatus() == theory::AreaLockStatus::Locked && !server->getAreaById(new_area)->invited().contains(id) && !checkPermission(ACLRole::BYPASS_LOCKS))
  {
    sendServerMessage("Area " + server->getAreaName(new_area) + " is locked.");
    return;
  }

  if (m_character != theory::NoCharacterId)
  {
    server->getAreaById(areaId())->changeCharacter(m_character, theory::NoCharacterId);
  }
  server->getAreaById(areaId())->removeClient(m_character, id);
  bool l_character_taken = false;
  if (server->getAreaById(new_area)->charactersTaken().contains(m_character))
  {
    setCharacter(theory::NoCharacterId);
    l_character_taken = true;
  }
  server->getAreaById(new_area)->addClient(m_character, id);
  setAreaId(new_area);

  theory::PenaltyPacket l_def_penalty;
  l_def_penalty.bar = theory::HealthBar::Defense;
  l_def_penalty.value = server->getAreaById(new_area)->defHP();
  shipPacket(l_def_penalty);

  theory::PenaltyPacket l_pro_penalty;
  l_pro_penalty.bar = theory::HealthBar::Prosecution;
  l_pro_penalty.value = server->getAreaById(new_area)->proHP();
  shipPacket(l_pro_penalty);

  theory::BackgroundPacket l_background;
  l_background.background = server->getAreaById(new_area)->background();
  l_background.side = server->getAreaById(new_area)->side();
  l_background.display = true;
  shipPacket(l_background);

  if (l_character_taken)
  {
    theory::CharacterAcceptedPacket l_accepted;
    l_accepted.character = theory::NoCharacterId;
    shipPacket(l_accepted);
  }
  server->getAreaById(areaId())->shipTimers(id);
  sendServerMessage("You moved to area " + server->getAreaName(areaId()));
  if (server->getAreaById(areaId())->sendAreaMessageOnJoin())
  {
    sendServerMessage(server->getAreaById(areaId())->areaMessage());
  }

  if (server->getAreaById(areaId())->lockStatus() == theory::AreaLockStatus::Spectatable)
  {
    sendServerMessage("Area " + server->getAreaName(areaId()) + " is spectate-only; to chat IC you will need to be invited by the CM.");
  }
}

bool kenji::AOClient::changeCharacter(theory::CharacterId char_id)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (char_id != theory::NoCharacterId)
  {
    if (m_is_charcursed)
    {
      if (!m_charcurse_list.contains(char_id))
      {
        return false;
      }
    }
    else if (!server->getCharacters().contains(char_id) && !l_area->iniswapAllowed())
    {
      return false;
    }
  }

  bool l_successfulChange = l_area->changeCharacter(m_character, char_id);

  if (char_id == theory::NoCharacterId)
  {
    setCharacter(theory::NoCharacterId);
    theory::CharacterAcceptedPacket l_accepted;
    l_accepted.character = theory::NoCharacterId;
    shipPacket(l_accepted);
    return true;
  }

  if (l_successfulChange == true)
  {
    setCharacter(char_id);
    m_pos = "";
    theory::CharacterAcceptedPacket l_accepted;
    l_accepted.character = char_id;
    shipPacket(l_accepted);
    return true;
  }
  return false;
}

void kenji::AOClient::changePosition(const QString &new_pos)
{
  m_pos = new_pos;
  sendServerMessage("Position changed to " + m_pos + ".");
  theory::SetPositionPacket l_position;
  if (!m_pos.isEmpty())
  {
    l_position.position = m_pos;
  }
  shipPacket(l_position);
}

void kenji::AOClient::handleCommand(QString command, int argc, QStringList argv)
{
  command = command.toLower();
  QString l_target_command = command;
  QList<ACLRole::Permission> l_permissions;

  // check for aliases
  const QList<CommandExtension> l_extensions = server->getCommandExtensionCollection()->getExtensions();
  for (const CommandExtension &i_extension : l_extensions)
  {
    if (i_extension.checkCommandNameAndAlias(command))
    {
      l_target_command = i_extension.getCommandName();
      l_permissions = i_extension.getPermissions();
      break;
    }
  }

  CommandInfo l_command = COMMANDS.value(l_target_command, {{ACLRole::NONE}, -1, &AOClient::cmdDefault});
  if (l_permissions.isEmpty())
  {
    l_permissions.append(l_command.acl_permissions);
  }

  bool l_has_permissions = false;
  for (const ACLRole::Permission i_permission : qAsConst(l_permissions))
  {
    if (checkPermission(i_permission))
    {
      l_has_permissions = true;
      break;
    }
  }
  if (!l_has_permissions)
  {
    sendServerMessage("You do not have permission to use that command.");
    return;
  }

  if (argc < l_command.minArgs)
  {
    sendServerMessage("Invalid command syntax.");
    sendServerMessage("The expected syntax for this command is: \n" + ConfigManager::commandHelp(command).usage);
    return;
  }

  (this->*(l_command.action))(argc, argv);
}

void kenji::AOClient::shipPacket(const theory::Packet &packet)
{
  if (m_session_status == SessionStatus::Inactive)
  {
    _queuedPackets.enqueue(packet.clonePacket());
    return;
  }

  m_socket->shipPacket(packet);
}

theory::Shared<theory::CargoSocket> kenji::AOClient::socket() const
{
  return m_socket;
}

void kenji::AOClient::setSocket(const theory::Shared<theory::CargoSocket> &socket)
{
  if (m_socket)
  {
    m_socket->disconnect(this);
  }

  m_socket = socket;
  connect(m_socket.get(), &theory::CargoSocket::disconnectedFromPeer, this, &AOClient::markInactive);
}

void kenji::AOClient::process(const theory::GoodbyePacket &)
{
  drop();
}

void kenji::AOClient::sendServerMessage(const QString &message)
{
  theory::ServerMessagePacket l_packet;
  l_packet.message = message;
  shipPacket(l_packet);
}

void kenji::AOClient::sendServerMessageArea(const QString &message)
{
  server->broadcastMessageToArea(message, areaId());
}

void kenji::AOClient::sendServerBroadcast(const QString &message)
{
  server->broadcastMessage(message);
}

bool kenji::AOClient::checkPermission(ACLRole::Permission f_permission) const
{
  if (f_permission == ACLRole::NONE)
  {
    return true;
  }

  if ((f_permission == ACLRole::CM) && server->getAreaById(areaId())->owners().contains(id))
  {
    return true; // I'm sorry for this hack.
  }

  if (!isAuthenticated())
  {
    return false;
  }

  if (ConfigManager::authType() == DataTypes::AuthType::SIMPLE)
  {
    return true;
  }

  const ACLRole l_role = server->getACLRolesHandler()->getRoleById(m_acl_role_id);
  return l_role.checkPermission(f_permission);
}

QString kenji::AOClient::getIpid() const
{
  return m_ipid;
}

QString kenji::AOClient::getHwid() const
{
  return m_hwid;
}

bool kenji::AOClient::isAuthenticated() const
{
  return m_authenticated;
}

QString kenji::AOClient::name() const
{
  return m_ooc_name;
}

void kenji::AOClient::setName(const QString &f_name)
{
  if (f_name != m_ooc_name)
  {
    m_ooc_name = f_name;
    Q_EMIT nameChanged(m_ooc_name);
  }
}

theory::AreaId kenji::AOClient::areaId() const
{
  return m_area_id;
}

void kenji::AOClient::setAreaId(const theory::AreaId f_area_id)
{
  if (f_area_id != m_area_id)
  {
    m_area_id = f_area_id;
    Q_EMIT areaIdChanged(m_area_id);
  }
}

theory::CharacterId kenji::AOClient::character() const
{
  return m_character;
}

void kenji::AOClient::setCharacter(const theory::CharacterId &f_character)
{
  if (f_character != m_character)
  {
    m_character = f_character;
    Q_EMIT characterChanged(m_character);
  }
}

std::optional<QString> kenji::AOClient::characterName() const
{
  return m_showname;
}

void kenji::AOClient::setCharacterName(const std::optional<QString> &f_showname)
{
  if (f_showname != m_showname)
  {
    m_showname = f_showname;
    Q_EMIT characterNameChanged(m_showname);
  }
}

theory::PlayerStatus kenji::AOClient::status() const
{
  return m_status;
}

void kenji::AOClient::setStatus(theory::PlayerStatus f_status)
{
  if (f_status != m_status)
  {
    m_status = f_status;
    Q_EMIT statusChanged(m_status);
  }
}

bool kenji::AOClient::isSpectator() const
{
  return m_character == theory::NoCharacterId;
}

void kenji::AOClient::onAfkTimeout()
{
  if (m_status != theory::PlayerStatus::Away)
  {
    sendServerMessage("You are now AFK.");
    setStatus(theory::PlayerStatus::Away);
  }
}

kenji::AOClient::AOClient(Server *p_server, ULogger &logger, InventoryRegistry &inventories, const theory::Shared<theory::CargoSocket> &socket, const QHostAddress &f_remote_ip, QObject *parent, theory::PlayerId playerId, theory::InventoryId f_inventory_id, MusicManager *p_manager)
    : QObject(parent)
    , id(playerId)
    , inventoryId(f_inventory_id)
    , m_remote_ip(f_remote_ip)
    , m_socket(socket)
    , m_music_manager(p_manager)
    , server(p_server)
    , m_logger(logger)
    , m_inventories(inventories)
{
  m_afk_timer = new QTimer(this);
  m_afk_timer->setSingleShot(true);
  connect(m_afk_timer, &QTimer::timeout, this, &AOClient::onAfkTimeout);

  m_session_timer = new QTimer(this);
  m_session_timer->setSingleShot(true);
  connect(m_session_timer, &QTimer::timeout, this, &AOClient::markExpired);

  registerSessionRoutes();
}

kenji::AOClient::~AOClient()
{}
