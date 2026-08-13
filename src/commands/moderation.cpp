#include "aoclient.h"

#include "area_data.h"
#include "command_extension.h"
#include "config_manager.h"
#include "db_manager.h"
#include "server.h"

// This file is for commands under the moderation category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdBan(int argc, QStringList argv)
{
  QString l_args_str = argv[2];
  if (argc > 3)
  {
    for (int i = 3; i < argc; i++)
    {
      l_args_str += " " + argv[i];
    }
  }

  BanInfo l_ban;

  BanDuration l_duration_seconds = 0;
  if (argv[1] == "perma")
  {
    l_duration_seconds = PermanentBanDuration;
  }
  else
  {
    l_duration_seconds = parseTime(argv[1]);
  }

  if (l_duration_seconds == -1)
  {
    sendServerMessage("Invalid time format. Format example: 1h30m");
    return;
  }

  l_ban.duration = l_duration_seconds;
  l_ban.ipid = argv[0];
  l_ban.reason = l_args_str;
  l_ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();
  bool l_ban_logged = false;
  int l_kick_counter = 0;

  switch (ConfigManager::authType())
  {
  case DataTypes::AuthType::SIMPLE:
    l_ban.moderator = "moderator";
    break;
  case DataTypes::AuthType::ADVANCED:
    l_ban.moderator = m_moderator_name;
    break;
  }

  const QList<AOClient *> l_targets = server->getClientsByIpid(l_ban.ipid);
  for (AOClient *l_client : l_targets)
  {
    if (!l_ban_logged)
    {
      l_ban.ip = l_client->m_remote_ip;
      l_ban.hdid = l_client->m_hwid;
      server->getDatabaseManager()->addBan(l_ban);
      sendServerMessage("Banned user with ipid " + l_ban.ipid + " for reason: " + l_ban.reason);
      l_ban_logged = true;
    }
    QString l_ban_duration = l_ban.until();
    int l_ban_id = server->getDatabaseManager()->getBanID(l_ban.ip);
    theory::ErrorPacket l_banned;
    l_banned.code = theory::ErrorPacket::Banned;
    l_banned.what = l_ban.reason + "\nID: " + QString::number(l_ban_id) + "\nUntil: " + l_ban_duration;
    l_client->shipPacket(l_banned);
    l_client->drop();
    l_kick_counter++;

    m_logger.logBan(l_ban.moderator, l_ban.ipid, l_ban_duration);
    if (ConfigManager::discordBanWebhookEnabled())
    {
      Q_EMIT server->banWebhookRequest(l_ban.ipid, l_ban.moderator, l_ban_duration, l_ban.reason, l_ban_id);
    }
  }

  if (l_kick_counter > 1)
  {
    sendServerMessage("Kicked " + QString::number(l_kick_counter) + " clients with matching ipids.");
  }

  // We're banning someone not connected.
  if (!l_ban_logged)
  {
    server->getDatabaseManager()->addBan(l_ban);
    sendServerMessage("Banned " + l_ban.ipid + " for reason: " + l_ban.reason);
  }
}

void kenji::AOClient::cmdKick(int argc, QStringList argv)
{
  QString l_target_ipid = argv[0];
  QString l_reason = argv[1];
  int l_kick_counter = 0;

  if (argc > 2)
  {
    for (int i = 2; i < argv.length(); i++)
    {
      l_reason += " " + argv[i];
    }
  }

  const QList<AOClient *> l_targets = server->getClientsByIpid(l_target_ipid);
  for (AOClient *l_client : l_targets)
  {
    theory::ErrorPacket l_kicked;
    l_kicked.code = theory::ErrorPacket::Banned;
    l_kicked.what = l_reason;
    l_client->shipPacket(l_kicked);
    l_client->drop();
    l_kick_counter++;
  }

  if (l_kick_counter > 0)
  {
    if (ConfigManager::authType() == DataTypes::AuthType::ADVANCED)
    {
      m_logger.logKick(m_moderator_name, l_target_ipid);
    }
    else
    {
      m_logger.logKick("Moderator", l_target_ipid);
    }
    sendServerMessage("Kicked " + QString::number(l_kick_counter) + " client(s) with ipid " + l_target_ipid + " for reason: " + l_reason);
  }
  else
  {
    sendServerMessage("User with ipid not found!");
  }
}

void kenji::AOClient::cmdMods(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QStringList l_entries;
  int l_online_count = 0;
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->m_authenticated)
    {
      l_entries << "---";
      if (ConfigManager::authType() != DataTypes::AuthType::SIMPLE)
      {
        l_entries << "Moderator: " + l_client->m_moderator_name;
        l_entries << "Role:" << l_client->m_acl_role_id;
      }
      l_entries << "OOC name: " + l_client->name();
      l_entries << "ID: " + QString::number(l_client->clientId());
      l_entries << "Area: " + QString::number(l_client->areaId());
      l_entries << "Character: " + l_client->character();
      l_online_count++;
    }
  }
  l_entries << "---";
  l_entries << "Total online: " << QString::number(l_online_count);
  sendServerMessage(l_entries.join("\n"));
}

void kenji::AOClient::cmdCommands(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QStringList l_entries;
  l_entries << "Allowed commands:";
  QMap<QString, CommandInfo>::const_iterator i;
  for (i = COMMANDS.constBegin(); i != COMMANDS.constEnd(); ++i)
  {
    const CommandInfo l_command = i.value();
    const CommandExtension l_extension = server->getCommandExtensionCollection()->getExtension(i.key());
    const QList<ACLRole::Permission> l_permissions = l_extension.getPermissions(l_command.acl_permissions);
    bool l_has_permission = false;
    for (const ACLRole::Permission i_permission : qAsConst(l_permissions))
    {
      if (checkPermission(i_permission))
      {
        l_has_permission = true;
        break;
      }
    }
    if (!l_has_permission)
    {
      continue;
    }

    QString l_info = "/" + i.key();
    const QStringList l_aliases = l_extension.getAliases();
    if (!l_aliases.isEmpty())
    {
      l_info += " [aka: " + l_aliases.join(", ") + "]";
    }
    l_entries << l_info;
  }
  sendServerMessage(l_entries.join("\n"));
}

void kenji::AOClient::cmdHelp(int argc, QStringList argv)
{
  CommandExtensionCollection *l_extension_collection = server->getCommandExtensionCollection();

  if (argc == 0)
  {
    sendServerMessage("Type /help <command> for help on a specific command, or /help all to list all commands.");
    return;
  }

  if (argc > 1)
  {
    sendServerMessage("Too many arguments. Please only use the command name.");
    return;
  }

  QString l_command_name = argv[0].toLower();

  auto l_check_for_permission = [this, l_extension_collection](const QString &f_command_name) -> bool {
    const QList<ACLRole::Permission> l_permissions = l_extension_collection->getExtension(f_command_name).getPermissions(COMMANDS.value(f_command_name).acl_permissions);
    for (const ACLRole::Permission i_permission : l_permissions)
    {
      if (checkPermission(i_permission))
      {
        return true;
      }
    }
    return false;
  };

  auto l_format_command = [l_extension_collection](const QString &f_command_name) -> QString {
    QString l_display_name = f_command_name;
    if (l_extension_collection->containsExtension(f_command_name))
    {
      l_display_name = l_extension_collection->getExtension(f_command_name).getDisplayName();
    }

    const QString l_description = ConfigManager::commandHelp(f_command_name).text;
    return "/" + l_display_name + "\n" + (l_description.isEmpty() ? QString("No details available.") : l_description);
  };

  QString l_message = "==Help==\n";

  // "all" is reserved
  if (l_command_name == "all")
  {
    QStringList l_entries;
    for (auto it = COMMANDS.cbegin(); it != COMMANDS.cend(); ++it)
    {
      if (l_check_for_permission(it.key()))
      {
        l_entries.append(l_format_command(it.key()));
      }
    }
    sendServerMessage(l_message + l_entries.join("\n\n"));
    return;
  }

  if (l_extension_collection->containsExtension(l_command_name))
  {
    l_command_name = l_extension_collection->getExtension(l_command_name).getCommandName();
  }

  if (!COMMANDS.contains(l_command_name))
  {
    sendServerMessage(l_message + "Unable to find the command " + l_command_name + ".");
    return;
  }

  if (!l_check_for_permission(l_command_name))
  {
    sendServerMessage(l_message + "You are not allowed to use the command " + l_command_name + ".");
    return;
  }

  sendServerMessage(l_message + l_format_command(l_command_name));
}

void kenji::AOClient::cmdMOTD(int argc, QStringList argv)
{
  Q_UNUSED(argc)
  Q_UNUSED(argv)

  sendServerMessage("=== MOTD ===\r\n" + ConfigManager::motd() + "\r\n=============");
}

void kenji::AOClient::cmdSetMOTD(int argc, QStringList argv)
{
  Q_UNUSED(argc)

  QString l_MOTD = argv.join(" ");
  ConfigManager::setMotd(l_MOTD);
  sendServerMessage("MOTD has been changed.");
}

void kenji::AOClient::cmdBans(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QStringList l_recent_bans;
  l_recent_bans << "Last 5 bans:";
  l_recent_bans << "-----";
  const QList<BanInfo> l_bans_list = server->getDatabaseManager()->getRecentBans();
  for (const BanInfo &l_ban : l_bans_list)
  {
    l_recent_bans << l_ban.toString();
    l_recent_bans << "-----";
  }
  sendServerMessage(l_recent_bans.join("\n"));
}

void kenji::AOClient::cmdUnBan(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok;
  int l_target_ban = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("Invalid ban ID.");
    return;
  }
  else if (server->getDatabaseManager()->invalidateBan(l_target_ban))
  {
    sendServerMessage("Successfully invalidated ban " + argv[0] + ".");
  }
  else
  {
    sendServerMessage("Couldn't invalidate ban " + argv[0] + ", are you sure it exists?");
  }
}

void kenji::AOClient::cmdAbout(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  theory::OocMessagePacket l_about;
  l_about.name = "The kenji dev team";
  l_about.message = "Thank you for using kenji! Made with love by scatterflower, with help from in1tiate, Salanto, and mangosarentliterature. kenji " + QCoreApplication::applicationVersion() + ". For documentation and reporting issues, see the source: https://github.com/AttorneyOnlineChaos/AO-CHAOS-Akashi";
  shipPacket(l_about);
}

void kenji::AOClient::cmdMute(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  int l_uid = argv[0].toInt(&conv_ok);
  if (!conv_ok)
  {
    sendServerMessage("Invalid user ID.");
    return;
  }

  AOClient *target = server->getClientByID(l_uid);

  if (target == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }

  if (target->m_is_muted)
  {
    sendServerMessage("That player is already muted!");
  }
  else
  {
    sendServerMessage("Muted player.");
    target->sendServerMessage("You were muted by a moderator. " + getReprimand());
  }
  target->m_is_muted = true;
}

void kenji::AOClient::cmdUnMute(int argc, QStringList argv)
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

  if (!l_target->m_is_muted)
  {
    sendServerMessage("That player is not muted!");
  }
  else
  {
    sendServerMessage("Unmuted player.");
    l_target->sendServerMessage("You were unmuted by a moderator. " + getReprimand(true));
  }
  l_target->m_is_muted = false;
}

void kenji::AOClient::cmdOocMute(int argc, QStringList argv)
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

  if (l_target->m_is_ooc_muted)
  {
    sendServerMessage("That player is already OOC muted!");
  }
  else
  {
    sendServerMessage("OOC muted player.");
    l_target->sendServerMessage("You were OOC muted by a moderator. " + getReprimand());
  }
  l_target->m_is_ooc_muted = true;
}

void kenji::AOClient::cmdOocUnMute(int argc, QStringList argv)
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

  if (!l_target->m_is_ooc_muted)
  {
    sendServerMessage("That player is not OOC muted!");
  }
  else
  {
    sendServerMessage("OOC unmuted player.");
    l_target->sendServerMessage("You were OOC unmuted by a moderator. " + getReprimand(true));
  }
  l_target->m_is_ooc_muted = false;
}

void kenji::AOClient::cmdBlockWtce(int argc, QStringList argv)
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

  if (l_target->m_is_wtce_blocked)
  {
    sendServerMessage("That player is already judge blocked!");
  }
  else
  {
    sendServerMessage("Revoked player's access to judge controls.");
    l_target->sendServerMessage("A moderator revoked your judge controls access. " + getReprimand());
  }
  l_target->m_is_wtce_blocked = true;
}

void kenji::AOClient::cmdUnBlockWtce(int argc, QStringList argv)
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

  if (!l_target->m_is_wtce_blocked)
  {
    sendServerMessage("That player is not judge blocked!");
  }
  else
  {
    sendServerMessage("Restored player's access to judge controls.");
    l_target->sendServerMessage("A moderator restored your judge controls access. " + getReprimand(true));
  }
  l_target->m_is_wtce_blocked = false;
}

void kenji::AOClient::cmdAllowBlankposting(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QString l_sender_name = name();
  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleBlankposting();
  if (l_area->blankpostingAllowed() == false)
  {
    sendServerMessageArea(l_sender_name + " has set blankposting in the area to forbidden.");
  }
  else
  {
    sendServerMessageArea(l_sender_name + " has set blankposting in the area to allowed.");
  }
}

void kenji::AOClient::cmdBanInfo(int argc, QStringList argv)
{
  QStringList l_ban_info;
  l_ban_info << ("Ban Info for " + argv[0]);
  l_ban_info << "-----";
  QString l_lookup_type;

  if (argc == 1)
  {
    l_lookup_type = "banid";
  }
  else if (argc == 2)
  {
    l_lookup_type = argv[1];
    if (!((l_lookup_type == "banid") || (l_lookup_type == "ipid") || (l_lookup_type == "hdid")))
    {
      sendServerMessage("Invalid ID type.");
      return;
    }
  }
  else
  {
    sendServerMessage("Invalid command.");
    return;
  }
  QString l_id = argv[0];
  const QList<BanInfo> l_bans = server->getDatabaseManager()->getBanInfo(l_lookup_type, l_id);
  for (const BanInfo &l_ban : l_bans)
  {
    l_ban_info << l_ban.toString();
    l_ban_info << "-----";
  }
  sendServerMessage(l_ban_info.join("\n"));
}

void kenji::AOClient::cmdReload(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  // Todo: Make this a signal when splitting AOClient and Server.
  server->reloadSettings();
  sendServerMessage("Reloaded configurations");
}

void kenji::AOClient::cmdForceImmediate(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleImmediate();
  QString l_state = l_area->forceImmediate() ? "on." : "off.";
  sendServerMessage("Forced immediate text processing in this area is now " + l_state);
}

void kenji::AOClient::cmdAllowIniswap(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleIniswap();
  QString state = l_area->iniswapAllowed() ? "allowed." : "disallowed.";
  sendServerMessage("Iniswapping in this area is now " + state);
}

void kenji::AOClient::cmdPermitSaving(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  AOClient *l_client = server->getClientByID(argv[0].toInt());
  if (l_client == nullptr)
  {
    sendServerMessage("Invalid ID.");
    return;
  }
  l_client->m_testimony_saving = true;
  sendServerMessage("Testimony saving has been enabled for client " + QString::number(l_client->clientId()));
}

void kenji::AOClient::cmdKickUid(int argc, QStringList argv)
{
  QString l_reason = argv[1];

  if (argc > 2)
  {
    for (int i = 2; i < argv.length(); i++)
    {
      l_reason += " " + argv[i];
    }
  }

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
  theory::ErrorPacket l_kicked;
  l_kicked.code = theory::ErrorPacket::Banned;
  l_kicked.what = l_reason;
  l_target->shipPacket(l_kicked);
  l_target->drop();
  sendServerMessage("Kicked client with UID " + argv[0] + " for reason: " + l_reason);
}

void kenji::AOClient::cmdUpdateBan(int argc, QStringList argv)
{
  bool conv_ok = false;
  int l_ban_id = argv[0].toInt(&conv_ok);
  if (!conv_ok)
  {
    sendServerMessage("Invalid ban ID.");
    return;
  }
  QVariant l_updated_info;
  if (argv[1] == "duration")
  {
    BanDuration l_duration_seconds = 0;
    if (argv[2] == "perma")
    {
      l_duration_seconds = PermanentBanDuration;
    }
    else
    {
      l_duration_seconds = parseTime(argv[2]);
    }
    if (l_duration_seconds == -1)
    {
      sendServerMessage("Invalid time format. Format example: 1h30m");
      return;
    }
    l_updated_info = QVariant(l_duration_seconds);
  }
  else if (argv[1] == "reason")
  {
    QString l_args_str = argv[2];
    if (argc > 3)
    {
      for (int i = 3; i < argc; i++)
      {
        l_args_str += " " + argv[i];
      }
    }
    l_updated_info = QVariant(l_args_str);
  }
  else
  {
    sendServerMessage("Invalid update type.");
    return;
  }
  if (!server->getDatabaseManager()->updateBan(l_ban_id, argv[1], l_updated_info))
  {
    sendServerMessage("There was an error updating the ban. Please confirm the ban ID is valid.");
    return;
  }
  sendServerMessage("Ban updated.");
}

void kenji::AOClient::cmdNotice(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  sendNotice(argv.join(" "));
}
void kenji::AOClient::cmdNoticeGlobal(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  sendNotice(argv.join(" "), true);
}

void kenji::AOClient::cmdKickOther(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  int l_kick_counter = 0;

  QList<AOClient *> l_target_clients;
  const QList<AOClient *> l_targets_hwid = server->getClientsByHwid(m_hwid);
  l_target_clients = server->getClientsByIpid(m_ipid);

  // Merge both lookups into one single list.)
  for (AOClient *l_target_candidate : qAsConst(l_targets_hwid))
  {
    if (!l_target_clients.contains(l_target_candidate))
    {
      l_target_clients.append(l_target_candidate);
    }
  }

  // The list is unique, we can only have on instance of the current client.
  l_target_clients.removeOne(this);
  for (AOClient *l_target_client : qAsConst(l_target_clients))
  {
    l_target_client->drop();
    l_kick_counter++;
  }
  sendServerMessage("Kicked " + QString::number(l_kick_counter) + " multiclients from the server.");
}

void kenji::AOClient::cmdDc(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  m_socket->close();
}
