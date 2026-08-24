#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "db_manager.h"
#include "music_manager.h"
#include "server.h"

#include <QQueue>

void kenji::AOClient::sendEvidenceList(AreaData *area) const
{
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() == areaId())
    {
      l_client->updateEvidenceList(area);
    }
  }
}

void kenji::AOClient::updateEvidenceList(AreaData *area)
{
  theory::EvidenceListPacket l_list;

  const QList<AreaData::Evidence> l_area_evidence = area->evidence();
  for (int i = 0; i < l_area_evidence.length(); i++)
  {
    const AreaData::Evidence &evidence = l_area_evidence.at(i);
    if (!checkPermission(ACLRole::CM) && area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM)
    {
      QRegularExpression l_regex("<owner=(.*?)>");
      QRegularExpressionMatch l_match = l_regex.match(evidence.description);
      if (l_match.hasMatch())
      {
        QStringList owners = l_match.captured(1).split(",");
        if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) && !owners.contains(m_pos, Qt::CaseSensitivity::CaseInsensitive))
        {
          continue;
        }
      }
      // no match = show it to all
    }

    theory::EvidenceItem l_item;
    l_item.id = i;
    l_item.evidence.name = evidence.name;
    l_item.evidence.description = evidence.description;
    l_item.evidence.image = evidence.image;
    l_list.items.append(l_item);
  }

  shipPacket(l_list);
}

QString kenji::AOClient::dezalgo(QString p_text)
{
  QRegularExpression rxp("([̴̵̶̷̸̡̢̧̨̛̖̗̘̙̜̝̞̟̠̣̤̥̦̩̪̫̬̭̮̯̰̱̲̳̹̺̻̼͇͈͉͍͎̀́̂̃̄̅̆̇̈̉̊̋̌̍̎̏̐̑̒̓̔̽̾̿̀́͂̓̈́͆͊͋͌̕̚ͅ͏͓͔͕͖͙͚͐͑͒͗͛ͣͤͥͦͧͨͩͪͫͬͭͮͯ͘͜͟͢͝͞͠͡])");
  QString filtered = p_text.replace(rxp, "");
  return filtered;
}

bool kenji::AOClient::checkEvidenceAccess(AreaData *area)
{
  switch (area->eviMod())
  {
  case AreaData::EvidenceMod::FFA:
    return true;
  case AreaData::EvidenceMod::CM:
  case AreaData::EvidenceMod::HIDDEN_CM:
    return checkPermission(ACLRole::CM);
  case AreaData::EvidenceMod::MOD:
    return m_authenticated;
  default:
    return false;
  }
}

void kenji::AOClient::updateJudgeLog(AreaData *area, AOClient *client, const QString &action)
{
  QString l_timestamp = QTime::currentTime().toString("hh:mm:ss");
  QString l_uid = QString::number(client->clientId());
  QString l_char_name = client->character();
  QString l_ipid = client->getIpid();
  QString l_message = action;
  QString l_logmessage = QString("[%1]: [%2] %3 (%4) %5").arg(l_timestamp, l_uid, l_char_name, l_ipid, l_message);
  area->appendJudgelog(l_logmessage);
}

void kenji::AOClient::loginAttempt(const QString &message)
{
  switch (ConfigManager::authType())
  {
  case DataTypes::AuthType::SIMPLE:
    if (message == ConfigManager::modpass())
    {
      theory::AuthStatePacket l_auth;
      l_auth.state = theory::AuthStatePacket::LoggedIn;
      shipPacket(l_auth);
      m_authenticated = true;
      m_acl_role_id = ACLRolesHandler::SUPER_ID;
    }
    else
    {
      theory::AuthStatePacket l_auth;
      l_auth.state = theory::AuthStatePacket::LoginFailed;
      shipPacket(l_auth); // Client: "Login unsuccessful."
    }
    m_logger.logLogin((character() + " " + characterName().value_or(QString())), name(), "Moderator", m_ipid, server->getAreaById(areaId())->name(), m_authenticated);
    break;
  case DataTypes::AuthType::ADVANCED:
    QStringList l_login = message.split(" ");
    if (l_login.size() < 2)
    {
      sendServerMessage("You must specify a username and a password");
      sendServerMessage("Exiting login prompt.");
      m_is_logging_in = false;
      return;
    }
    QString username = l_login[0];
    QString password = l_login[1];
    if (server->getDatabaseManager()->authenticate(username, password))
    {
      m_authenticated = true;
      m_acl_role_id = server->getDatabaseManager()->getACL(username);
      m_moderator_name = username;
      theory::AuthStatePacket l_auth;
      l_auth.state = theory::AuthStatePacket::LoggedIn;
      shipPacket(l_auth);
      sendServerMessage("Welcome, " + username);
    }
    else
    {
      theory::AuthStatePacket l_auth;
      l_auth.state = theory::AuthStatePacket::LoginFailed;
      shipPacket(l_auth);
    }
    m_logger.logLogin((character() + " " + characterName().value_or(QString())), name(), username, m_ipid, server->getAreaById(areaId())->name(), m_authenticated);
    break;
  }
  sendServerMessage("Exiting login prompt.");
  m_is_logging_in = false;
  return;
}
