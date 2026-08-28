#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

// This file is for functions used by various commands, defined in the command helper function category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdDefault(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  sendServerMessage("Invalid command.");
  return;
}

QStringList kenji::AOClient::buildAreaList(int area_idx)
{
  QStringList entries;
  QString area_name = server->getAreaName(area_idx);
  AreaData *area = server->getAreaById(area_idx);
  entries.append("=== " + area_name + " ===");
  switch (area->lockStatus())
  {
  case theory::AreaLockStatus::Locked:
    entries.append("[LOCKED]");
    break;
  case theory::AreaLockStatus::Spectatable:
    entries.append("[SPECTATABLE]");
    break;
  case theory::AreaLockStatus::Unlocked:
  default:
    break;
  }
  entries.append("[" + QString::number(area->playerCount()) + " users][" + AreaData::map_statuses.key(area->status()) + "]");
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() == area_idx)
    {
      QString char_entry = "[" + QString::number(l_client->clientId()) + "] " + l_client->character().toString();
      if (l_client->character() == theory::NoCharacterId)
      {
        char_entry += "Spectator";
      }
      if (l_client->characterName())
      {
        char_entry += " (" + l_client->characterName().value() + ")";
      }
      if (l_client->status() == theory::PlayerStatus::Away)
      {
        char_entry += " [AFK]";
      }
      if (area->owners().contains(l_client->clientId()))
      {
        char_entry.insert(0, "[CM] ");
      }
      if (m_authenticated)
      {
        char_entry += " (" + l_client->getIpid() + "): " + l_client->name();
      }
      entries.append(char_entry);
    }
  }
  return entries;
}

int kenji::AOClient::genRand(int min, int max)
{
  return QRandomGenerator::system()->bounded(min, max + 1);
}

void kenji::AOClient::diceThrower(int sides, int dice, bool p_roll, int roll_modifier)
{
  if (sides < 0 || dice < 0 || sides > ConfigManager::diceMaxValue() || dice > ConfigManager::diceMaxDice())
  {
    sendServerMessage("Dice or side number out of bounds.");
    return;
  }
  QStringList results;
  for (int i = 1; i <= dice; i++)
  {
    results.append(QString::number(AOClient::genRand(1, sides) + roll_modifier));
  }
  QString total_results = results.join(" ");
  if (p_roll)
  {
    if (roll_modifier)
    {
      sendServerMessage("You rolled a " + QString::number(dice) + "d" + QString::number(sides) + "+" + QString::number(roll_modifier) + ". Results: " + total_results);
    }
    else
    {
      sendServerMessage("You rolled a " + QString::number(dice) + "d" + QString::number(sides) + ". Results: " + total_results);
    }
    return;
  }
  if (roll_modifier)
  {
    sendServerMessageArea(name() + " rolled a " + QString::number(dice) + "d" + QString::number(sides) + "+" + QString::number(roll_modifier) + ". Results: " + total_results);
  }
  else
  {
    sendServerMessageArea(name() + " rolled a " + QString::number(dice) + "d" + QString::number(sides) + ". Results: " + total_results);
  }
}

QString kenji::AOClient::getAreaTimer(int area_idx, int timer_idx)
{
  Timer *l_timer;
  if (timer_idx == 0)
  {
    l_timer = server->globalTimer();
  }
  else
  {
    l_timer = server->getAreaById(area_idx)->timer(timer_idx);
  }
  QString l_timer_name = "Timer " + QString::number(timer_idx);

  if (l_timer == nullptr)
  {
    return "Invalid timer ID.";
  }

  if (l_timer->state() == theory::TimerState::NotRunning)
  {
    return l_timer_name + " is inactive.";
  }

  QTime l_current_time = QTime(0, 0).addMSecs(l_timer->remaining());
  if (l_timer->state() == theory::TimerState::Paused)
  {
    return l_timer_name + " is paused at " + l_current_time.toString("hh:mm:ss.zzz");
  }

  return l_timer_name + " is at " + l_current_time.toString("hh:mm:ss.zzz");
}

long long kenji::AOClient::parseTime(const QString &input)
{
  QRegularExpression l_regex("(?:(?:(?<year>.*?)y)*(?:(?<week>.*?)w)*(?:(?<day>.*?)d)*(?:(?<hr>.*?)h)*(?:(?<min>.*?)m)*(?:(?<sec>.*?)s)*)");
  QRegularExpressionMatch match = l_regex.match(input);
  QString str_year, str_week, str_hour, str_day, str_minute, str_second;
  int year, week, day, hour, minute, second;

  str_year = match.captured("year");
  str_week = match.captured("week");
  str_day = match.captured("day");
  str_hour = match.captured("hr");
  str_minute = match.captured("min");
  str_second = match.captured("sec");

  bool l_is_well_formed = false;
  QString concat_str(str_year + str_week + str_day + str_hour + str_minute + str_second);
  concat_str.toInt(&l_is_well_formed);

  if (!l_is_well_formed)
  {
    return -1;
  }

  year = str_year.toInt();
  week = str_week.toInt();
  day = str_day.toInt();
  hour = str_hour.toInt();
  minute = str_minute.toInt();
  second = str_second.toInt();

  long long l_total = 0;
  l_total += 31622400 * year;
  l_total += 604800 * week;
  l_total += 86400 * day;
  l_total += 3600 * hour;
  l_total += 60 * minute;
  l_total += second;

  if (l_total < 0)
  {
    return -1;
  }

  return l_total;
}

QString kenji::AOClient::getReprimand(bool f_positive)
{
  QStringList l_list;
  if (f_positive)
  {
    l_list = ConfigManager::praiseList();
  }
  else
  {
    l_list = ConfigManager::reprimandsList();
  }

  if (l_list.isEmpty())
  {
    return QString();
  }
  return l_list.at(genRand(0, l_list.size() - 1));
}

bool kenji::AOClient::checkPasswordRequirements(const QString &f_username, const QString &f_password)
{
  if (!ConfigManager::passwordRequirements())
  {
    return true;
  }

  if (ConfigManager::passwordMinLength() > f_password.length())
  {
    return false;
  }

  if (ConfigManager::passwordMaxLength() < f_password.length() && ConfigManager::passwordMaxLength() != 0)
  {
    return false;
  }

  if (ConfigManager::passwordRequireMixCase())
  {
    if (f_password.toLower() == f_password)
    {
      return false;
    }
    if (f_password.toUpper() == f_password)
    {
      return false;
    }
  }

  if (ConfigManager::passwordRequireNumbers())
  {
    QRegularExpression regex("[0123456789]");
    QRegularExpressionMatch match = regex.match(f_password);
    if (!match.hasMatch())
    {
      return false;
    }
  }

  if (ConfigManager::passwordRequireSpecialCharacters())
  {
    QRegularExpression regex(R"re([~!@#$%^&*_+=`|(){}\[\]:;"'<>,.?/\\-])re");
    QRegularExpressionMatch match = regex.match(f_password);
    if (!match.hasMatch())
    {
      return false;
    }
  }

  if (!ConfigManager::passwordCanContainUsername())
  {
    if (f_password.contains(f_username))
    {
      return false;
    }
  }

  return true;
}

void kenji::AOClient::sendNotice(const QString &f_notice, bool f_global)
{
  QString l_message = "A moderator sent this ";
  if (f_global)
  {
    l_message += "server-wide ";
  }
  l_message += "notice:\n\n" + f_notice;

  if (f_global)
  {
    server->broadcastMessage(l_message, theory::ServerMessagePacket::Notice);
  }
  else
  {
    server->broadcastMessageToArea(l_message, areaId(), theory::ServerMessagePacket::Notice);
  }
}
