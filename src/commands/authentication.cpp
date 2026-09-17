#include "ao_client.h"

#include "config_manager.h"
#include "core/logging.h"
#include "kenji_defs.h"
#include "server.h"
#include "server_database.h"

// This file is for commands under the authentication category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdListPerms(int argc, QStringList argv)
{
  const ACLRole l_role = server->getACLRolesHandler()->getRoleById(m_acl_role_id);

  ACLRole l_target_role = l_role;
  QStringList l_message;
  if (argc == 0)
  {
    l_message.append("You have been given the following permissions:");
  }
  else
  {
    if (!l_role.checkPermission(ACLRole::MODIFY_USERS))
    {
      sendServerMessage("You do not have permission to view other users' permissions.");
      return;
    }

    bool l_ok;
    const theory::PlayerId l_target_id = argv[0].toInt(&l_ok);
    if (!l_ok)
    {
      sendServerMessage("Invalid player ID.");
      return;
    }

    AOClient *l_target = server->getClientByID(l_target_id);
    if (l_target == nullptr)
    {
      sendServerMessage("No client with that ID found.");
      return;
    }

    l_message.append("Player " + argv[0] + " has the following permissions:");
    l_target_role = server->getACLRolesHandler()->getRoleById(l_target->m_acl_role_id);
  }

  if (l_target_role.getPermissions() == ACLRole::NONE)
  {
    l_message.append("NONE");
  }
  else if (l_target_role.checkPermission(ACLRole::SUPER))
  {
    l_message.append("SUPER (Be careful! This grants the user all permissions.)");
  }
  else
  {
    const QList<ACLRole::Permission> l_permissions = ACLRole::PERMISSION_CAPTIONS.keys();
    for (const ACLRole::Permission i_permission : l_permissions)
    {
      if (l_target_role.checkPermission(i_permission))
      {
        l_message.append(ACLRole::PERMISSION_CAPTIONS.value(i_permission));
      }
    }
  }

  sendServerMessage(l_message.join("\n"));
}

void kenji::AOClient::cmdSetPerms(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  const QString l_target_acl = argv[1];
  if (!server->getACLRolesHandler()->roleExists(l_target_acl))
  {
    sendServerMessage("That role doesn't exist!");
    return;
  }

  if (l_target_acl == ACLRolesHandler::SUPER_ID && !checkPermission(ACLRole::SUPER))
  {
    sendServerMessage("You aren't allowed to set that role!");
    return;
  }

  bool l_ok;
  const theory::PlayerId l_target_id = argv[0].toInt(&l_ok);
  if (!l_ok)
  {
    sendServerMessage("Invalid player ID.");
    return;
  }

  AOClient *l_target = server->getClientByID(l_target_id);
  if (l_target == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }

  if (l_target->isGuest())
  {
    sendServerMessage("Guests can't be given a role.");
    return;
  }

  const theory::UserId l_user_id = l_target->userId;
  if (ConfigManager::superUserIds().contains(l_user_id))
  {
    sendServerMessage("That player's role is fixed in the server configuration.");
    return;
  }

  if (const std::optional<theory::IOError> l_error = server->database().setRole(l_user_id, l_target_acl))
  {
    zWarning(log::commands) << QStringLiteral("/setperms %1: %2").arg(argv[0], l_error->toString());
    sendServerMessage("The role could not be saved.");
    return;
  }

  const QList<AOClient *> l_clients = server->getClientsByUserId(l_user_id);
  for (AOClient *l_client : l_clients)
  {
    l_client->applyRole(l_target_acl);
  }

  sendServerMessage("Successfully applied role " + l_target_acl + " to player " + argv[0]);
}

void kenji::AOClient::cmdRemovePerms(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  bool l_ok;
  const theory::PlayerId l_target_id = argv[0].toInt(&l_ok);
  if (!l_ok)
  {
    sendServerMessage("Invalid player ID.");
    return;
  }

  AOClient *l_target = server->getClientByID(l_target_id);
  if (l_target == nullptr)
  {
    sendServerMessage("No client with that ID found.");
    return;
  }

  if (l_target->isGuest())
  {
    sendServerMessage("Guests have no role to remove.");
    return;
  }

  const theory::UserId l_user_id = l_target->userId;
  if (ConfigManager::superUserIds().contains(l_user_id))
  {
    sendServerMessage("That player's role is fixed in the server configuration.");
    return;
  }

  if (!server->database().role(l_user_id))
  {
    sendServerMessage("That player has no role.");
    return;
  }

  if (const std::optional<theory::IOError> l_error = server->database().clearRole(l_user_id))
  {
    zWarning(log::commands) << QStringLiteral("/removeperms %1: %2").arg(argv[0], l_error->toString());
    sendServerMessage("The role could not be removed.");
    return;
  }

  const QList<AOClient *> l_clients = server->getClientsByUserId(l_user_id);
  for (AOClient *l_client : l_clients)
  {
    l_client->clearRole();
  }

  sendServerMessage("Successfully removed the role of player " + argv[0]);
}
