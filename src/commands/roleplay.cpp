#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "core/logging.h"
#include "kenji_log.h"
#include "server.h"

// This file is for commands under the roleplay category in aoclient.h
// Be sure to register the command in the header before adding it here!

void kenji::AOClient::cmdFlip(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  QString l_sender_name = name();
  QStringList l_faces = {"heads", "tails"};
  QString l_face = l_faces[AOClient::genRand(0, 1)];
  sendServerMessageArea(l_sender_name + " flipped a coin and got " + l_face + ".");
}

void kenji::AOClient::cmdRoll(int argc, QStringList argv)
{
  int l_sides = 6;
  int l_dice = 1;

  if (argc >= 1)
  {
    if (argv[0].contains('d'))
    {
      QStringList l_arguments = argv[0].split('d');

      bool l_dice_ok;
      bool l_sides_ok;
      l_dice = l_arguments[0].toInt(&l_dice_ok);
      l_sides = l_arguments[1].toInt(&l_sides_ok);

      if (argv[0].contains('+'))
      {
        bool l_mod_ok;
        QStringList l_modifier = l_arguments[1].split('+');
        if (l_modifier.size() < 2)
        {
          sendServerMessage("Invalid dice notation.");
          return;
        }
        int modifier = l_modifier[1].toInt(&l_mod_ok);
        l_sides = l_modifier[0].toInt(&l_sides_ok);

        if (l_mod_ok && l_dice_ok && l_sides_ok)
        {
          diceThrower(l_sides, l_dice, false, modifier);
        }
        else
        {
          sendServerMessage("Invalid dice notation.");
        }
        return;
      }
      else if (argv[0].contains('-'))
      {
        bool l_mod_ok;
        QStringList l_modifier = l_arguments[1].split('-');
        if (l_modifier.size() < 2)
        {
          sendServerMessage("Invalid dice notation.");
          return;
        }
        int modifier = l_modifier[1].toInt(&l_mod_ok);
        l_sides = l_modifier[0].toInt(&l_sides_ok);

        if (l_mod_ok && l_dice_ok && l_sides_ok)
        {
          diceThrower(l_sides, l_dice, false, -modifier);
        }
        else
        {
          sendServerMessage("Invalid dice notation.");
        }
        return;
      }
      else if (l_dice_ok && l_sides_ok)
      {
        diceThrower(l_sides, l_dice, false);
        return;
      }
      else
      {
        sendServerMessage("Invalid dice notation.");
        return;
      }
    }
    else
    {
      l_sides = qBound(1, argv[0].toInt(), ConfigManager::diceMaxValue());
    }
  }
  if (argc == 2)
  {
    l_dice = qBound(1, argv[1].toInt(), ConfigManager::diceMaxDice());
  }
  diceThrower(l_sides, l_dice, false);
}

void kenji::AOClient::cmdRollA(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_dice_name = argv.join(" ");

  if (ConfigManager::diceFaces(l_dice_name).isEmpty())
  {
    zWarning(log::commands) << "Unknown dice.";
    sendServerMessage("Unknown dice.");
  }
  else
  {
    QString l_response = ConfigManager::diceFaces(l_dice_name).at((genRand(0, ConfigManager::diceFaces(l_dice_name).size() - 1)));
    QString l_sender_name = name();

    sendServerMessageArea(l_sender_name + " rolled from the \"" + l_dice_name + "\" set and got: " + l_response);
  }
}

void kenji::AOClient::cmdRollP(int argc, QStringList argv)
{
  int l_sides = 6;
  int l_dice = 1;

  if (argc >= 1)
  {
    if (argv[0].contains('d'))
    {
      QStringList l_arguments = argv[0].split('d');

      bool l_dice_ok;
      bool l_sides_ok;
      l_dice = l_arguments[0].toInt(&l_dice_ok);
      l_sides = l_arguments[1].toInt(&l_sides_ok);

      if (argv[0].contains('+'))
      {
        bool l_mod_ok;
        QStringList l_modifier = l_arguments[1].split('+');
        if (l_modifier.size() < 2)
        {
          sendServerMessage("Invalid dice notation.");
          return;
        }
        int modifier = l_modifier[1].toInt(&l_mod_ok);
        l_sides = l_modifier[0].toInt(&l_sides_ok);

        if (l_mod_ok && l_dice_ok && l_sides_ok)
        {
          diceThrower(l_sides, l_dice, true, modifier);
        }
        else
        {
          sendServerMessage("Invalid dice notation.");
        }
        return;
      }
      else if (argv[0].contains('-'))
      {
        bool l_mod_ok;
        QStringList l_modifier = l_arguments[1].split('-');
        if (l_modifier.size() < 2)
        {
          sendServerMessage("Invalid dice notation.");
          return;
        }
        int modifier = l_modifier[1].toInt(&l_mod_ok);
        l_sides = l_modifier[0].toInt(&l_sides_ok);

        if (l_mod_ok && l_dice_ok && l_sides_ok)
        {
          diceThrower(l_sides, l_dice, true, -modifier);
        }
        else
        {
          sendServerMessage("Invalid dice notation.");
        }
        return;
      }
      else if (l_dice_ok && l_sides_ok)
      {
        diceThrower(l_sides, l_dice, true);
        return;
      }
      else
      {
        sendServerMessage("Invalid dice notation.");
        return;
      }
    }
    else
    {
      l_sides = qBound(1, argv[0].toInt(), ConfigManager::diceMaxValue());
    }
  }
  if (argc == 2)
  {
    l_dice = qBound(1, argv[1].toInt(), ConfigManager::diceMaxDice());
  }
  diceThrower(l_sides, l_dice, true);
}

void kenji::AOClient::cmdTimer(int argc, QStringList argv)
{
  AreaData *l_area = server->getAreaById(areaId());

  // Called without arguments
  // Shows a brief of all timers
  if (argc == 0)
  {
    QStringList l_timers;
    l_timers.append("Currently active timers:");
    for (theory::TimerId l_timer_id = 0; l_timer_id < theory::TimerCount; l_timer_id++)
    {
      l_timers.append(getAreaTimer(l_area->index(), l_timer_id));
    }
    sendServerMessage(l_timers.join("\n"));
    return;
  }

  // Called with more than one argument
  bool ok;
  theory::TimerId l_timer_id = argv[0].toInt(&ok);
  if (!ok || l_timer_id < 0 || l_timer_id >= theory::TimerCount)
  {
    sendServerMessage("Invalid timer ID. Timer ID must be a whole number between 0 and " + QString::number(theory::TimerCount - 1) + ".");
    return;
  }

  // Called with one argument
  // Shows the status of one timer
  if (argc == 1)
  {
    sendServerMessage(getAreaTimer(l_area->index(), l_timer_id));
    return;
  }

  // Called with more than one argument
  // Updates the state of a timer

  // Select the proper timer
  Timer *l_requested_timer;
  if (l_timer_id == 0)
  {
    if (!checkPermission(ACLRole::GLOBAL_TIMER))
    {
      sendServerMessage("You are not authorized to alter the global timer.");
      return;
    }
    l_requested_timer = server->globalTimer();
  }
  else
  {
    l_requested_timer = l_area->timer(l_timer_id);
  }

  if (l_requested_timer == nullptr)
  {
    sendServerMessage("Invalid timer ID. This area has no timer " + QString::number(l_timer_id) + ".");
    return;
  }

  // Set the timer's time remaining if the second
  // argument is a valid time
  QTime l_requested_time = QTime::fromString(argv[1], "hh:mm:ss");
  if (l_requested_time.isValid())
  {
    l_requested_timer->stop();
    l_requested_timer->setDuration(QTime(0, 0).msecsTo(l_requested_time));
    l_requested_timer->setVisible(true);
    l_requested_timer->start();
    sendServerMessage("Set timer " + QString::number(l_timer_id) + " to " + argv[1] + ".");
    return;
  }
  // Otherwise, update the state of the timer
  else
  {
    if (argv[1] == "start")
    {
      l_requested_timer->setVisible(true);
      if (l_requested_timer->state() == theory::TimerState::Paused)
      {
        l_requested_timer->pause(false);
      }
      else
      {
        l_requested_timer->start();
      }
      sendServerMessage("Started timer " + QString::number(l_timer_id) + ".");
    }
    else if (argv[1] == "pause" || argv[1] == "stop")
    {
      l_requested_timer->pause(true);
      sendServerMessage("Paused timer " + QString::number(l_timer_id) + ".");
    }
    else if (argv[1] == "hide" || argv[1] == "unset")
    {
      l_requested_timer->stop();
      l_requested_timer->setVisible(false);
      sendServerMessage("Hid timer " + QString::number(l_timer_id) + ".");
    }
  }
}

void kenji::AOClient::cmdNoteCard(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  AreaData *l_area = server->getAreaById(areaId());
  QString l_notecard = argv.join(" ");
  l_area->addNotecard(m_character.toString(), l_notecard);
  sendServerMessageArea(m_character.toString() + " wrote a note card.");
}

void kenji::AOClient::cmdNoteCardClear(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  if (!l_area->addNotecard(m_character.toString(), QString()))
  {
    sendServerMessageArea(m_character.toString() + " erased their note card.");
  }
}

void kenji::AOClient::cmdNoteCardReveal(int argc, QStringList argv)
{
  Q_UNUSED(argc);
  Q_UNUSED(argv);

  AreaData *l_area = server->getAreaById(areaId());
  const QStringList l_notecards = l_area->getNotecards();

  if (l_notecards.isEmpty())
  {
    sendServerMessage("There are no cards to reveal in this area.");
    return;
  }

  QString l_message("Note cards have been revealed.\n");
  l_message.append(l_notecards.join(""));

  sendServerMessageArea(l_message);
}

void kenji::AOClient::cmd8Ball(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  if (ConfigManager::magic8BallAnswers().isEmpty())
  {
    zWarning(log::commands) << "8ball.txt is empty!";
    sendServerMessage("8ball.txt is empty.");
  }
  else
  {
    QString l_response = ConfigManager::magic8BallAnswers().at((genRand(0, ConfigManager::magic8BallAnswers().size() - 1)));
    QString l_sender_name = name();
    QString l_sender_message = argv.join(" ");

    sendServerMessageArea(l_sender_name + " asked the magic 8-ball, \"" + l_sender_message + "\" and the answer is: " + l_response);
  }
}

void kenji::AOClient::cmdSubTheme(int argc, QStringList argv)
{
  Q_UNUSED(argc);

  QString l_subtheme = argv.join(" ");
  theory::SubthemePacket l_subtheme_packet;
  l_subtheme_packet.subtheme = l_subtheme;
  const QList<AOClient *> l_clients = server->getClients();
  for (AOClient *l_client : l_clients)
  {
    if (l_client->areaId() == areaId())
    {
      l_client->shipPacket(l_subtheme_packet);
    }
  }
  sendServerMessageArea("Subtheme was set to " + l_subtheme);
}
