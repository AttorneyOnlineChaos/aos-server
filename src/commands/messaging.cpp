#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

// This file is for commands under the messaging category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdPos(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  changePosition(argv[0]);
  updateEvidenceList(server->getAreaById(areaId()));
}

void kenji::AOClient::cmdForcePos(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok;
  QList<AOClient *> l_targets;
  theory::PlayerId l_target_id = argv[1].toInt(&ok);
  int l_forced_clients = 0;
  if (!ok && argv[1] != "*")
  {
    sendServerMessage("That does not look like a valid ID.");
    return;
  }
  else if (ok)
  {
    AOClient *l_target_client = server->getClientByID(l_target_id);
    if (l_target_client != nullptr)
    {
      l_targets.append(l_target_client);
    }
    else
    {
      sendServerMessage("Target ID not found!");
      return;
    }
  }

  else if (argv[1] == "*")
  { // force all clients in the area
    const QList<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients)
    {
      if (l_client->areaId() == areaId())
      {
        l_targets.append(l_client);
      }
    }
  }
  for (AOClient *l_target : l_targets)
  {
    l_target->sendServerMessage("Position forcibly changed by CM.");
    l_target->changePosition(argv[0]);
    l_forced_clients++;
  }
  sendServerMessage("Forced " + QString::number(l_forced_clients) + " into pos " + argv[0] + ".");
}

void kenji::AOClient::cmdG(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_sender_name = name();
  QString l_sender_area = server->getAreaName(areaId());
  QString l_sender_message = argv.join(" ");
  // Better readability thanks to AwesomeAim.
  theory::OocMessagePacket l_mod_packet;
  l_mod_packet.name = "[G][" + m_ipid + "][" + l_sender_area + "]" + l_sender_name;
  l_mod_packet.message = l_sender_message;
  theory::OocMessagePacket l_user_packet;
  l_user_packet.name = "[G][" + l_sender_area + "]" + l_sender_name;
  l_user_packet.message = l_sender_message;
  server->broadcastIf(l_user_packet, [](const AOClient &client) { return client.m_global_enabled && !client.isAuthenticated(); });
  server->broadcastIf(l_mod_packet, [](const AOClient &client) { return client.m_global_enabled && client.isAuthenticated(); });
  return;
}

void kenji::AOClient::cmdNeed(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_sender_area = server->getAreaName(areaId());
  QString l_sender_message = argv.join(" ");
  theory::ServerMessagePacket l_advert;
  l_advert.message = "=== Advert ===\n[" + l_sender_area + "] needs " + l_sender_message + ".";
  server->broadcastIf(l_advert, [](const AOClient &client) { return client.m_advert_enabled; });
}

void kenji::AOClient::cmdSwitch(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  theory::CharacterId l_selected_char_id = theory::CharacterId(argv.join(" "));
  if (l_selected_char_id == theory::NoCharacterId)
  {
    sendServerMessage("That does not look like a valid character.");
    return;
  }
  if (!changeCharacter(l_selected_char_id))
  {
    sendServerMessage("The character you picked is either taken or invalid.");
  }
}

void kenji::AOClient::cmdRandomChar(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  QList<theory::CharacterId> l_available = server->getCharacters();
  for (const theory::CharacterId &l_taken_char : l_area->charactersTaken())
  {
    l_available.removeOne(l_taken_char);
  }
  if (l_available.isEmpty())
  {
    sendServerMessage("There are no available characters.");
    return;
  }
  changeCharacter(l_available.at(genRand(0, l_available.size() - 1)));
}

void kenji::AOClient::cmdToggleGlobal(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  m_global_enabled = !m_global_enabled;
  QString l_str_en = m_global_enabled ? "shown" : "hidden";
  sendServerMessage("Global chat set to " + l_str_en);
}

void kenji::AOClient::cmdPM(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok;
  theory::PlayerId l_target_id = argv.takeFirst().toInt(&ok); // using takeFirst removes the ID from our list of arguments...
  if (!ok)
  {
    sendServerMessage("That does not look like a valid ID.");
    return;
  }
  AOClient *l_target_client = server->getClientByID(l_target_id);
  if (l_target_client == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }
  if (l_target_client->m_pm_mute)
  {
    sendServerMessage("That user is not recieving PMs.");
    return;
  }
  QString l_message = argv.join(" "); //...which means it will not end up as part of the message
  l_target_client->sendServerMessage("Message from " + name() + " (" + QString::number(playerId()) + "): " + l_message);
  sendServerMessage("PM sent to " + QString::number(l_target_id) + ". Message: " + l_message);
}

void kenji::AOClient::cmdAnnounce(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  sendServerBroadcast("=== Announcement ===\r\n" + argv.join(" ") + "\r\n=============");
}

void kenji::AOClient::cmdM(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_sender_name = name();
  QString l_sender_message = argv.join(" ");
  theory::OocMessagePacket l_packet;
  l_packet.name = "[M]" + l_sender_name;
  l_packet.message = l_sender_message;
  server->broadcastIf(l_packet, [](const AOClient &client) { return client.checkPermission(ACLRole::MODCHAT); });
}

void kenji::AOClient::cmdGM(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_sender_name = name();
  QString l_sender_area = server->getAreaName(areaId());
  QString l_sender_message = argv.join(" ");
  theory::OocMessagePacket l_packet;
  l_packet.name = "[G][" + l_sender_area + "]" + "[" + l_sender_name + "][M]";
  l_packet.message = l_sender_message;
  server->broadcastIf(l_packet, [](const AOClient &client) { return client.checkPermission(ACLRole::MODCHAT); });
}

void kenji::AOClient::cmdLM(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_sender_name = name();
  QString l_sender_message = argv.join(" ");
  theory::OocMessagePacket l_packet;
  l_packet.name = "[" + l_sender_name + "][M]";
  l_packet.message = l_sender_message;
  server->broadcastToArea(l_packet, areaId());
}

void kenji::AOClient::cmdGimp(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (l_target->m_is_gimped)
  {
    sendServerMessage("That player is already gimped!");
  }
  else
  {
    sendServerMessage("Gimped player.");
    l_target->sendServerMessage("You have been gimped! " + getReprimand());
  }
  l_target->m_is_gimped = true;
}

void kenji::AOClient::cmdUnGimp(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (!(l_target->m_is_gimped))
  {
    sendServerMessage("That player is not gimped!");
  }
  else
  {
    sendServerMessage("Ungimped player.");
    l_target->sendServerMessage("A moderator has ungimped you! " + getReprimand(true));
  }
  l_target->m_is_gimped = false;
}

void kenji::AOClient::cmdDisemvowel(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (l_target->m_is_disemvoweled)
  {
    sendServerMessage("That player is already disemvoweled!");
  }
  else
  {
    sendServerMessage("Disemvoweled player.");
    l_target->sendServerMessage("You have been disemvoweled! " + getReprimand());
  }
  l_target->m_is_disemvoweled = true;
}

void kenji::AOClient::cmdUnDisemvowel(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (!(l_target->m_is_disemvoweled))
  {
    sendServerMessage("That player is not disemvoweled!");
  }
  else
  {
    sendServerMessage("Undisemvoweled player.");
    l_target->sendServerMessage("A moderator has undisemvoweled you! " + getReprimand(true));
  }
  l_target->m_is_disemvoweled = false;
}

void kenji::AOClient::cmdShake(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (l_target->m_is_shaken)
  {
    sendServerMessage("That player is already shaken!");
  }
  else
  {
    sendServerMessage("Shook player.");
    l_target->sendServerMessage("A moderator has shaken your words! " + getReprimand());
  }
  l_target->m_is_shaken = true;
}

void kenji::AOClient::cmdUnShake(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (!(l_target->m_is_shaken))
  {
    sendServerMessage("That player is not shaken!");
  }
  else
  {
    sendServerMessage("Unshook player.");
    l_target->sendServerMessage("A moderator has unshook you! " + getReprimand(true));
  }
  l_target->m_is_shaken = false;
}

void kenji::AOClient::cmdMedieval(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (l_target->m_is_medieval)
  {
    sendServerMessage("That player is already speaking Ye Olde English!");
  }
  else
  {
    sendServerMessage("It is done, sire.");
    l_target->sendServerMessage("Forsooth! Thine speech will henceforth be Ye Olde!");
  }
  l_target->m_is_medieval = true;
}

void kenji::AOClient::cmdUnMedieval(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (!(l_target->m_is_medieval))
  {
    sendServerMessage("That player is not shaken!");
  }
  else
  {
    sendServerMessage("Un-medieval'd player.");
    l_target->sendServerMessage("Hark! Thine speech hast been returneth to normal.");
  }
  l_target->m_is_medieval = false;
}

void kenji::AOClient::cmdMutePM(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  m_pm_mute = !m_pm_mute;
  QString l_str_en = m_pm_mute ? "muted" : "unmuted";
  sendServerMessage("PM's are now " + l_str_en);
}

void kenji::AOClient::cmdToggleAdverts(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  m_advert_enabled = !m_advert_enabled;
  QString l_str_en = m_advert_enabled ? "on" : "off";
  sendServerMessage("Advertisements turned " + l_str_en);
}

void kenji::AOClient::cmdAfk(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  setStatus(theory::PlayerStatus::Away);
  sendServerMessage("You are now AFK.");
}

void kenji::AOClient::cmdCharCurse(int argc, QStringList argv)
{
  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (l_target->m_is_charcursed)
  {
    sendServerMessage("That player is already charcursed!");
    return;
  }

  if (argc == 1)
  {
    if (l_target->isSpectator())
    {
      sendServerMessage("That player is a spectator!");
      return;
    }
    l_target->m_charcurse_list.append(l_target->character());
  }
  else
  {
    argv.removeFirst();
    QStringList l_char_names = argv.join(" ").split(",");

    QList<theory::CharacterId> l_curse_list;
    for (const QString &l_char_name : qAsConst(l_char_names))
    {
      theory::CharacterId char_id = theory::CharacterId(l_char_name);
      if (!server->getCharacters().contains(char_id))
      {
        sendServerMessage("Could not find character: " + l_char_name);
        return;
      }
      l_curse_list.append(char_id);
    }
    l_target->m_charcurse_list = l_curse_list;
  }

  // Kick back to char select screen
  if (!l_target->m_charcurse_list.contains(l_target->character()))
  {
    l_target->changeCharacter(theory::NoCharacterId);
  }

  l_target->m_is_charcursed = true;

  l_target->sendCharacterList();
  l_target->sendCharacterSelection();

  l_target->sendServerMessage("You have been charcursed!");
  sendServerMessage("Charcursed player.");
}

void kenji::AOClient::cmdUnCharCurse(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool conv_ok = false;
  theory::PlayerId l_uid = argv[0].toInt(&conv_ok);
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

  if (!l_target->m_is_charcursed)
  {
    sendServerMessage("That player is not charcursed!");
    return;
  }
  l_target->m_is_charcursed = false;
  l_target->m_charcurse_list.clear();
  l_target->sendCharacterList();
  l_target->sendCharacterSelection();
  sendServerMessage("Uncharcursed player.");
  l_target->sendServerMessage("You were uncharcursed.");
}

void kenji::AOClient::cmdCharSelect(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  changeCharacter(theory::NoCharacterId);
}

void kenji::AOClient::cmdForceCharSelect(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok = false;
  theory::PlayerId l_target_id = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("This ID does not look valid. Please use the client ID.");
    return;
  }

  AOClient *l_target = server->getClientByID(l_target_id);

  if (l_target == nullptr)
  {
    sendServerMessage("Unable to locate client with ID " + QString::number(l_target_id) + ".");
    return;
  }

  l_target->changeCharacter(theory::NoCharacterId);
  sendServerMessage("Client has been forced into character select!");
}

void kenji::AOClient::cmdA(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok;
  theory::AreaId l_area_id = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("This does not look like a valid AreaID.");
    return;
  }

  AreaData *l_area = server->getAreaById(l_area_id);
  if (l_area == nullptr)
  {
    sendServerMessage("This does not look like a valid AreaID.");
    return;
  }
  if (!l_area->owners().contains(playerId()))
  {
    sendServerMessage("You are not CM in that area.");
    return;
  }

  argv.removeAt(0);
  QString l_sender_name = name();
  QString l_ooc_message = argv.join(" ");
  theory::OocMessagePacket l_packet;
  l_packet.name = "[CM]" + l_sender_name;
  l_packet.message = l_ooc_message;
  server->broadcastToArea(l_packet, l_area_id);
}

void kenji::AOClient::cmdS(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  theory::AreaId l_all_areas = server->getAreaCount() - 1;
  QString l_sender_name = name();
  QString l_ooc_message = argv.join(" ");

  for (theory::AreaId i = 0; i <= l_all_areas; i++)
  {
    if (server->getAreaById(i)->owners().contains(playerId()))
    {
      theory::OocMessagePacket l_packet;
      l_packet.name = "[CM]" + l_sender_name;
      l_packet.message = l_ooc_message;
      server->broadcastToArea(l_packet, i);
    }
  }
}

void kenji::AOClient::cmdFirstPerson(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  m_first_person = !m_first_person;
  QString l_str_en = m_first_person ? "enabled" : "disabled";
  sendServerMessage("First person mode " + l_str_en + ".");
}
