#include "ao_client.h"

#include "config_manager.h"
#include "server.h"

#include <QRegularExpression>

void kenji::AOClient::process(const theory::OocMessagePacket &packet)
{
  if (m_is_ooc_muted)
  {
    sendServerMessage("You are OOC muted, and cannot speak.");
    return;
  }

  QString l_name = dezalgo(packet.name).replace(QRegularExpression("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&"), ""); // no fucky wucky shit here
  if (l_name.trimmed().replace("​", "").isEmpty() || l_name == ConfigManager::serverNickname())           // impersonation & empty name protection
  {
    return;
  }

  if (l_name.length() > ConfigManager::maxNameLength())
  {
    sendServerMessage("Your name is too long! Please limit it to under " + QString::number(ConfigManager::maxNameLength()) + " characters.");
    return;
  }

  setName(l_name);

  if (m_is_logging_in)
  {
    loginAttempt(packet.message);
    return;
  }

  QString l_message = dezalgo(packet.message);

  if (l_message.length() == 0 || l_message.length() > ConfigManager::maxTextLength())
  {
    return;
  }

  if (!ConfigManager::filterList().isEmpty())
  {
    for (const QString &regex : ConfigManager::filterList())
    {
      QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
      l_message.replace(re, "❌");
    }
  }

  if (l_message.at(0) == '/')
  {
    QStringList l_cmd_argv = l_message.split(" ", Qt::SkipEmptyParts);
    QString l_command = l_cmd_argv[0].trimmed().toLower();
    l_command = l_command.right(l_command.length() - 1);
    l_cmd_argv.removeFirst();
    int l_cmd_argc = l_cmd_argv.length();

    handleCommand(l_command, l_cmd_argc, l_cmd_argv);
    m_logger.logCMD((character() + " " + characterName().value_or(QString())), m_ipid, name(), l_command, l_cmd_argv, server->getAreaById(areaId())->name());
    return;
  }
  else
  {
    theory::OocMessagePacket l_broadcast;
    l_broadcast.name = name();
    l_broadcast.message = l_message;
    server->broadcastToArea(l_broadcast, areaId());
  }
  m_logger.logOOC(server->getAreaById(areaId())->name(), m_ipid, name(), QString::number(clientId()), (character() + " " + characterName().value_or(QString())), l_message);
}
