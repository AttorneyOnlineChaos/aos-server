#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

// This file is for commands under the area category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdCM(int argc, QStringList argv)
{
  QString l_sender_name = name();
  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->isProtected() && !checkPermission(ACLRole::SUPER))
  {
    sendServerMessage("This area is protected, you may not become CM in this area.");
    return;
  }
  else if (l_area->owners().isEmpty())
  { // no one owns this area, and it's not protected
    l_area->addOwner(id);
    sendServerMessageArea(l_sender_name + " is now CM in this area.");
  }
  else if (!l_area->owners().contains(id))
  { // there is already a CM, and it isn't us
    sendServerMessage("You cannot become a CM in this area when someone else is. You must be CM'ed by an existing one.");
  }
  else if (argc == 1)
  { // we are CM, and we want to make ID argv[0] also CM
    bool ok;
    AOClient *l_owner_candidate = server->getClientByID(argv[0].toInt(&ok));
    if (!ok)
    {
      sendServerMessage("That doesn't look like a valid ID.");
      return;
    }
    if (l_owner_candidate == nullptr)
    {
      sendServerMessage("Unable to find client with ID " + argv[0] + ".");
      return;
    }
    if (l_area->owners().contains(l_owner_candidate->id))
    {
      sendServerMessage("User is already a CM in this area.");
      return;
    }
    l_area->addOwner(l_owner_candidate->id);
    sendServerMessageArea(l_owner_candidate->name() + " is now CM in this area.");
  }
  else
  {
    sendServerMessage("You are already a CM in this area.");
  }
}

void kenji::AOClient::cmdUnCM(int argc, QStringList argv)
{
  AreaData *l_area = server->getAreaById(areaId());
  theory::PlayerId l_uid;

  if (l_area->owners().isEmpty())
  {
    sendServerMessage("There are no CMs in this area.");
    return;
  }
  else if (argc == 0)
  {
    l_uid = id;
    sendServerMessage("You are no longer CM in this area.");
  }
  else if (checkPermission(ACLRole::UNCM) && argc >= 1)
  {
    // Remove all owners except yourself
    if (argv[0] == "all")
    {
      QList<theory::PlayerId> owners = l_area->owners();
      for (theory::PlayerId uid : owners)
      {
        if (uid != id)
        {
          l_area->removeOwner(uid);
          AOClient *l_target = server->getClientByID(uid);
          if (l_target != nullptr)
          {
            l_target->sendServerMessage("You have been unCMed.");
          }
        }
      }
      sendServerMessage("All CMs except yourself have been unCMed.");
      return;
    }

    bool conv_ok = false;
    l_uid = argv[0].toInt(&conv_ok);
    if (!conv_ok)
    {
      sendServerMessage("Invalid user ID.");
      return;
    }
    if (!l_area->owners().contains(l_uid))
    {
      sendServerMessage("That user is not CMed.");
      return;
    }
    AOClient *l_target = server->getClientByID(l_uid);
    if (l_target == nullptr)
    {
      sendServerMessage("No client with that ID found.");
      return;
    }
    sendServerMessage(l_target->name() + " was successfully unCMed.");
    l_target->sendServerMessage("You have been unCMed by a moderator.");
  }
  else
  {
    sendServerMessage("You do not have permission to unCM others. Only yourself.");
    return;
  }

  l_area->removeOwner(l_uid);
}

void kenji::AOClient::cmdInvite(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  AreaData *l_area = server->getAreaById(areaId());
  bool ok;
  theory::PlayerId l_invited_id = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("That does not look like a valid ID.");
    return;
  }

  AOClient *target_client = server->getClientByID(l_invited_id);
  if (target_client == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }
  else if (!l_area->invite(l_invited_id))
  {
    sendServerMessage("That ID is already on the invite list.");
    return;
  }
  sendServerMessage("You invited ID " + argv[0]);
  target_client->sendServerMessage("You were invited and given access to " + l_area->name());
}

void kenji::AOClient::cmdUnInvite(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  AreaData *l_area = server->getAreaById(areaId());
  bool ok;
  theory::PlayerId l_uninvited_id = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("That does not look like a valid ID.");
    return;
  }

  AOClient *target_client = server->getClientByID(l_uninvited_id);
  if (target_client == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }
  else if (l_area->owners().contains(l_uninvited_id))
  {
    sendServerMessage("You cannot uninvite a CM!");
    return;
  }
  else if (!l_area->uninvite(l_uninvited_id))
  {
    sendServerMessage("That ID is not on the invite list.");
    return;
  }
  sendServerMessage("You uninvited ID " + argv[0]);
  target_client->sendServerMessage("You were uninvited from " + l_area->name());
}

void kenji::AOClient::cmdLock(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *area = server->getAreaById(areaId());
  if (area->lockStatus() == theory::AreaLockStatus::Locked)
  {
    sendServerMessage("This area is already locked.");
    return;
  }
  sendServerMessageArea("This area is now locked.");
  area->lock();
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() == areaId())
    {
      area->invite(l_client->id);
    }
  }
}

void kenji::AOClient::cmdSpectatable(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->lockStatus() == theory::AreaLockStatus::Spectatable)
  {
    sendServerMessage("This area is already in spectate mode.");
    return;
  }
  sendServerMessageArea("This area is now spectatable.");
  l_area->spectatable();
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() == areaId())
    {
      l_area->invite(l_client->id);
    }
  }
}

void kenji::AOClient::cmdUnLock(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->lockStatus() == theory::AreaLockStatus::Unlocked)
  {
    sendServerMessage("This area is not locked.");
    return;
  }
  sendServerMessageArea("This area is now unlocked.");
  l_area->unlock();
}

void kenji::AOClient::cmdGetAreas(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QStringList l_entries;
  l_entries.append("\n== Currently Online: " + QString::number(server->getPlayerCount()) + " ==");
  for (theory::AreaId i = 0; i < server->getAreaCount(); i++)
  {
    if (server->getAreaById(i)->playerCount() > 0)
    {
      QStringList l_cur_area_lines = buildAreaList(i);
      l_entries.append(l_cur_area_lines);
    }
  }
  sendServerMessage(l_entries.join("\n"));
}

void kenji::AOClient::cmdGetArea(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QStringList l_entries = buildAreaList(areaId());
  sendServerMessage(l_entries.join("\n"));
}

void kenji::AOClient::cmdArea(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool ok;
  theory::AreaId l_new_area = argv[0].toInt(&ok);
  if (!ok || l_new_area >= server->getAreaCount() || l_new_area < 0)
  {
    sendServerMessage("That does not look like a valid area ID.");
    return;
  }
  changeArea(l_new_area);
}

void kenji::AOClient::cmdAreaKick(int argc, QStringList argv)
{
  AreaData *l_area = server->getAreaById(areaId());

  theory::AreaId target_area_id = 0; // Default to first area of the server

  // Check if a target area is provided
  if (argc >= 2)
  {
    if (!checkPermission(ACLRole::KICK))
    {
      sendServerMessage("You do not have permission to kick to specific areas. Just the first area as CM. (/areakick [ID]).");
      return;
    }

    bool ok;
    target_area_id = argv[1].toInt(&ok);
    if (!ok || target_area_id < 0 || target_area_id >= server->getAreaCount())
    {
      sendServerMessage("That does not look like a valid area ID.");
      return;
    }
  }

  AreaData *target_area = server->getAreaById(target_area_id);

  if (argv[0] == "all")
  {
    const QList<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients)
    {
      if (l_client->areaId() == areaId() && l_client->id != id)
      {
        if (!server->getAreaById(areaId())->owners().contains(l_client->id))
        {
          l_client->changeArea(target_area_id);
          l_area->uninvite(l_client->id);
          l_client->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
        }
      }
    }
    sendServerMessage("All clients kicked to area " + target_area->displayName() + ".");
    return;
  }

  // Without secondary area argument
  bool ok;
  theory::PlayerId l_idx = argv[0].toInt(&ok);
  if (!ok)
  {
    sendServerMessage("That does not look like a valid ID.");
    return;
  }
  if (server->getAreaById(areaId())->owners().contains(l_idx))
  {
    sendServerMessage("You cannot kick another CM!");
    return;
  }
  AOClient *l_client_to_kick = server->getClientByID(l_idx);
  if (l_client_to_kick == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }
  else if (l_client_to_kick->areaId() != areaId())
  {
    sendServerMessage("That client is not in this area.");
    return;
  }
  l_client_to_kick->changeArea(target_area_id);
  l_area->uninvite(l_client_to_kick->id);
  l_client_to_kick->sendServerMessage("You have been kicked to area " + target_area->displayName() + ".");
  sendServerMessage("Client " + argv[0] + " kicked to area " + target_area->displayName() + ".");
}

void kenji::AOClient::cmdSetBackground(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString f_background = argv.join(" ");
  AreaData *area = server->getAreaById(areaId());
  if (m_authenticated || !area->bgLocked())
  {
    if (area->lockStatus() == theory::AreaLockStatus::Spectatable && !area->invited().contains(id) && !checkPermission(ACLRole::BYPASS_LOCKS))
    {
      sendServerMessage("Spectators are blocked from changing the background.");
      return;
    }
    if (server->getBackgrounds().contains(f_background, Qt::CaseInsensitive) || area->ignoreBgList() == true)
    {
      area->setBackground(f_background);
      theory::BackgroundPacket l_background;
      l_background.background = f_background;
      l_background.side = area->side();
      l_background.display = true;
      server->broadcastToArea(l_background, areaId());
      theory::MusicChangedPacket l_ambience;
      l_ambience.character = theory::NoCharacterId;
      l_ambience.characterName = characterName();
      l_ambience.channel = theory::MusicChannel::Ambient;
      l_ambience.loop = true;
      QString ambience_name = ConfigManager::ambience()->value(f_background + "/ambience").toString();
      if (!ambience_name.trimmed().isEmpty())
      {
        l_ambience.track = ambience_name;
      }
      server->broadcastToArea(l_ambience, areaId());
      sendServerMessageArea(m_character.toString() + " changed the background to " + f_background);
    }
    else
    {
      sendServerMessage("Invalid background name.");
    }
  }
  else
  {
    sendServerMessage("This area's background is locked.");
  }
}

void kenji::AOClient::cmdSetSide(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  AreaData *area = server->getAreaById(areaId());
  if (area->bgLocked())
  {
    sendServerMessage("This area's background is locked.");
    return;
  }

  const QString l_joined_side = argv.join(" ");
  std::optional<QString> l_side;
  if (!l_joined_side.isEmpty())
  {
    l_side = l_joined_side;
  }
  area->setSide(l_side);
  theory::BackgroundPacket l_background;
  l_background.background = area->background();
  l_background.side = l_side;
  l_background.display = true;
  server->broadcastToArea(l_background, areaId());
  if (l_side)
  {
    sendServerMessageArea(m_character.toString() + " locked the background side to " + l_side.value());
  }
  else
  {
    sendServerMessageArea(m_character.toString() + " unlocked the background side");
  }
}

void kenji::AOClient::cmdBgLock(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());

  if (l_area->bgLocked() == false)
  {
    l_area->toggleBgLock();
  }

  sendServerMessageArea(m_character.toString() + " locked the background.");
}

void kenji::AOClient::cmdBgUnlock(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());

  if (l_area->bgLocked() == true)
  {
    l_area->toggleBgLock();
  }

  sendServerMessageArea(m_character.toString() + " unlocked the background.");
}

void kenji::AOClient::cmdStatus(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  static const QHash<QString, theory::AreaStatus> l_shortcuts = {
      {"RP", theory::AreaStatus::Roleplay},
      {"LFP", theory::AreaStatus::LookingForPlayers},
  };

  AreaData *l_area = server->getAreaById(areaId());
  const QString l_arg = argv[0].toUpper();

  theory::AreaStatus l_status;
  if (l_shortcuts.contains(l_arg))
  {
    l_status = l_shortcuts.value(l_arg);
  }
  else if (AreaData::map_statuses.contains(l_arg))
  {
    l_status = AreaData::map_statuses.value(l_arg);
  }
  else
  {
    sendServerMessage(QStringLiteral("Unknown status '%1'; expected one of: %2").arg(l_arg, AreaData::map_statuses.keys().join(", ")));
    return;
  }

  l_area->changeStatus(l_status);
  sendServerMessageArea(m_character.toString() + " changed status to " + l_arg);
}

void kenji::AOClient::cmdJudgeLog(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->judgelog().isEmpty())
  {
    sendServerMessage("There have been no judge actions in this area.");
    return;
  }
  QString l_message = l_area->judgelog().join("\n");
  // Judgelog contains an IPID, so we shouldn't send that unless the caller has appropriate permissions
  if (checkPermission(ACLRole::KICK) || checkPermission(ACLRole::BAN))
  {
    sendServerMessage(l_message);
  }
  else
  {
    QString filteredmessage = l_message.remove(QRegularExpression("[(].*[)]")); // Filter out anything between two parentheses. This should only ever be the IPID
    sendServerMessage(filteredmessage);
  }
}

void kenji::AOClient::cmdIgnoreBgList(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleIgnoreBgList();
  QString l_state = l_area->ignoreBgList() ? "ignored." : "enforced.";
  sendServerMessage("BG list in this area is now " + l_state);
}

void kenji::AOClient::cmdAreaMessage(int argc, QStringList argv)
{
  AreaData *l_area = server->getAreaById(areaId());
  if (argc == 0)
  {
    sendServerMessage(l_area->areaMessage());
    return;
  }

  if (argc >= 1)
  {
    l_area->changeAreaMessage(argv.join(" "));
    sendServerMessage("Updated this area's message.");
  }
}

void kenji::AOClient::cmdToggleAreaMessageOnJoin(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleAreaMessageJoin();
  QString l_state = l_area->sendAreaMessageOnJoin() ? "enabled." : "disabled.";
  sendServerMessage("Sending message on area join is now " + l_state);
}

void kenji::AOClient::cmdToggleWtce(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleWtceAllowed();
  QString l_state = l_area->isWtceAllowed() ? "enabled." : "disabled.";
  sendServerMessage("Using testimony animations is now " + l_state);
}

void kenji::AOClient::cmdToggleShouts(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleShoutAllowed();
  QString l_state = l_area->isShoutAllowed() ? "enabled." : "disabled.";
  sendServerMessage("Using shouts is now " + l_state);
}

void kenji::AOClient::cmdClearAreaMessage(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->clearAreaMessage();
  if (l_area->sendAreaMessageOnJoin()) // Turn off the automatic sending.
  {
    cmdToggleAreaMessageOnJoin(0, QStringList{}); // Dummy values.
  }
}

void kenji::AOClient::cmdWebfiles(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  const QList<AOClient *> l_clients = server->getClients();
  QStringList l_weblinks;
  QList<theory::CharacterId> l_listed_characters;
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() != areaId() || l_client->isSpectator() || l_listed_characters.contains(l_client->character()))
    {
      continue;
    }

    l_listed_characters.append(l_client->character());
    l_weblinks.append("https://attorneyonline.github.io/webDownloader/index.html?char=" + l_client->character().toString());
  }
  sendServerMessage("Character files:\n" + l_weblinks.join("\n"));
}

void kenji::AOClient::cmdMedievalMode(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  l_area->toggleMedievalMode();
  QString l_state = l_area->isMedievalMode() ? "enabled." : "disabled.";
  sendServerMessageArea("Hear ye, hear ye! Medieval Mode is now " + l_state);
}
