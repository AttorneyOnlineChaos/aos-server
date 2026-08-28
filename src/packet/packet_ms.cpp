#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "medieval_parser.h"
#include "server.h"

#include <QDebug>
#include <QRegularExpression>

#include <random>

void kenji::AOClient::process(const theory::IcMessagePacket &packet)
{
  AreaData *l_area = server->getAreaById(areaId());

  if (m_is_muted)
  {
    sendServerMessage("You cannot speak while muted.");
    return;
  }

  if (!l_area->isMessageAllowed() || !server->isMessageAllowed())
  {
    return;
  }

  auto l_validated = validateIcMessage(packet);
  if (!l_validated)
  {
    return;
  }
  theory::IcMessagePacket l_message = l_validated.value();

  if (m_pos != "")
  {
    l_message.side = m_pos;
  }

  // Check if evidence was presented and we need to handle HIDDEN_CM mode
  if (l_message.evidenceId != theory::NoEvidenceId && l_area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM)
  {
    if (l_area->getVisibleIndexByEvidenceIndex(l_message.evidenceId, m_pos, checkPermission(ACLRole::CM)) <= 0)
    {
      l_message.evidenceId = theory::NoEvidenceId;
    }
    else
    {
      l_area->setEvidenceOwnerToAll(l_message.evidenceId);
      // Update evidence list for all clients in the area
      sendEvidenceList(l_area);
    }
  }

  server->broadcastToArea(l_message, areaId());

  m_logger.logIC(l_area->name(), m_ipid, name(), QString::number(clientId()), (m_character.toString() + " " + characterName().value_or(QString())), m_last_message);
  l_area->updateLastICMessage(l_message);

  l_area->startMessageFloodguard(ConfigManager::messageFloodguard());
  server->startMessageFloodguard(ConfigManager::globalMessageFloodguard());
}

std::optional<theory::IcMessagePacket> kenji::AOClient::validateIcMessage(const theory::IcMessagePacket &packet)
{
  // Welcome to the super cursed server-side IC chat validation hell

  if (isSpectator())
  {
    // Spectators cannot use IC
    return std::nullopt;
  }
  AreaData *l_area = server->getAreaById(areaId());
  if (l_area->lockStatus() == theory::AreaLockStatus::Spectatable && !l_area->invited().contains(clientId()) && !checkPermission(ACLRole::BYPASS_LOCKS))
  {
    // Non-invited players cannot speak in spectatable areas
    return std::nullopt;
  }

  auto isTestimonyJumpCommand = [](const QString &message) {
    QRegularExpression jump("(?<arrow>>|<)(?<int>\\d+)");
    return jump.match(message);
  };

  theory::IcMessagePacket l_message = packet;

  // desk modifier
  switch (packet.deskMod)
  {
  default:
    return std::nullopt;
  case theory::DeskMod::Hidden:
  case theory::DeskMod::Shown:
  case theory::DeskMod::EmoteOnly:
  case theory::DeskMod::PreAnimationOnly:
  case theory::DeskMod::EmoteOnlyExpanded:
  case theory::DeskMod::PreAnimationOnlyExpanded:
    break;
  }

  // emote
  m_emote = packet.emote;
  if (m_first_person)
  {
    m_emote = "";
  }
  l_message.emote = m_emote;

  // message text
  if (packet.message.size() > ConfigManager::maxIcTextLength())
  {
    return std::nullopt;
  }

  // Doublepost prevention. Has to ignore blankposts and testimony commands.
  QString l_incoming_msg = dezalgo(packet.message.trimmed());
  QRegularExpressionMatch match = isTestimonyJumpCommand(l_incoming_msg);
  bool msg_is_testimony_cmd = (match.hasMatch() || l_incoming_msg == ">" || l_incoming_msg == "<");
  if (!m_last_message.isEmpty()           // If the last message you sent isn't empty,
      && l_incoming_msg == m_last_message // and it matches the one you're sending,
      && !msg_is_testimony_cmd)           // and it's not a testimony command,
  {
    return std::nullopt; // get it the hell outta here!
  }

  if (l_incoming_msg == "" && l_area->blankpostingAllowed() == false)
  {
    sendServerMessage("Blankposting has been forbidden in this area.");
    return std::nullopt;
  }

  m_last_message = l_incoming_msg;

  if (!ConfigManager::filterList().isEmpty())
  {
    for (const QString &regex : ConfigManager::filterList())
    {
      QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
      l_incoming_msg.replace(re, "❌");
    }
  }

  if (m_is_gimped)
  {
    const QStringList l_gimp_list = ConfigManager::gimpList();
    if (!l_gimp_list.isEmpty())
    {
      l_incoming_msg = l_gimp_list.at(genRand(0, l_gimp_list.size() - 1));
    }
  }

  if (m_is_medieval || l_area->isMedievalMode())
  {
    QString l_medieval_message = server->getMedievalParser()->degrootify(l_incoming_msg);
    l_incoming_msg = l_medieval_message;
  }

  if (m_is_shaken)
  {
    QStringList l_parts = l_incoming_msg.split(" ");

    std::random_device rng;
    std::mt19937 urng(rng());
    std::shuffle(l_parts.begin(), l_parts.end(), urng);

    l_incoming_msg = l_parts.join(" ");
  }

  if (m_is_disemvoweled)
  {
    QString l_disemvoweled_message = l_incoming_msg.remove(QRegularExpression("[AEIOUaeiou]")); // john madden
    l_incoming_msg = l_disemvoweled_message;
  }

  l_message.message = l_incoming_msg;

  // side
  // this is validated clientside so w/e
  if (l_area->side())
  {
    l_message.side = l_area->side().value();
  }

  if (m_pos != packet.side)
  {
    m_pos = packet.side;
    m_pos.replace("../", "").replace("..\\", "");
    updateEvidenceList(server->getAreaById(areaId()));
  }

  // emote modifier
  switch (packet.emoteMode)
  {
  default:
    return std::nullopt;
  case theory::EmoteMode::Idle:
  case theory::EmoteMode::PreAnimation:
  case theory::EmoteMode::Zoom:
  case theory::EmoteMode::PreAnimationZoom:
    break;
  }

  // char id
  if (packet.character != m_character)
  {
    return std::nullopt;
  }

  // objection modifier
  if (!l_area->isShoutAllowed())
  {
    if (packet.shout.type != theory::ShoutType::None)
    {
      sendServerMessage("Shouts have been disabled in this area.");
    }
    l_message.shout = theory::Shout{};
  }

  // evidence
  if (l_message.evidenceId >= l_area->evidence().length())
  {
    return std::nullopt;
  }

  m_flipping = packet.flip;

  // showname
  std::optional<QString> l_incoming_showname;
  if (packet.characterName)
  {
    l_incoming_showname = dezalgo(packet.characterName->trimmed());
  }
  if (l_incoming_showname && !(l_incoming_showname.value() == m_character.toString() || l_incoming_showname->isEmpty()) && !l_area->shownameAllowed())
  {
    sendServerMessage("Shownames are not allowed in this area!");
    return std::nullopt;
  }
  if (l_incoming_showname && l_incoming_showname->length() > ConfigManager::maxIcNameLength())
  {
    sendServerMessage("Your showname is too long! Please limit it to under " + QString::number(ConfigManager::maxIcNameLength()) + " characters");
    return std::nullopt;
  }

  // if the raw input is not empty but the trimmed input is, use a single space
  if (l_incoming_showname && l_incoming_showname->isEmpty())
  {
    l_incoming_showname = " ";
  }
  l_message.characterName = l_incoming_showname;
  setCharacterName(l_incoming_showname);

  // pairing
  m_pairing_with = packet.pair ? packet.pair->character : theory::NoCharacterId;
  m_offset_x = packet.offsetX;
  m_offset_y = packet.offsetY;

  bool l_pairing = false;
  if (l_message.pair)
  {
    for (int l_client_id : l_area->joinedIDs())
    {
      AOClient *l_client = server->getClientByID(l_client_id);
      if (l_client == nullptr)
      {
        continue;
      }
      if (l_client->m_pairing_with == m_character && m_pairing_with != m_character && l_client->character() == m_pairing_with && l_client->m_pos == m_pos)
      {
        l_message.pair->character = l_client->character();
        l_message.pair->emote = l_client->m_emote;
        l_message.pair->offsetX = l_client->m_offset_x;
        l_message.pair->offsetY = l_client->m_offset_y;
        l_message.pair->flip = l_client->m_flipping;
        l_pairing = true;
      }
    }
  }
  if (!l_pairing)
  {
    l_message.pair.reset();
  }

  // immediate text processing
  if (l_area->forceImmediate())
  {
    if (l_message.emoteMode == theory::EmoteMode::PreAnimation)
    {
      l_message.emoteMode = theory::EmoteMode::Idle;
      l_message.immediate = true;
    }
    else if (l_message.emoteMode == theory::EmoteMode::PreAnimationZoom)
    {
      l_message.emoteMode = theory::EmoteMode::Zoom;
      l_message.immediate = true;
    }
  }

  // additive
  if (l_area->lastICMessage().character != m_character)
  {
    l_message.additive = false;
  }
  else if (l_message.additive)
  {
    l_message.message.prepend(" ");
  }

  // Testimony playback
  QString client_name = name();
  if (client_name == "")
  {
    client_name = m_character.toString(); // fallback in case of empty ooc name
  }
  if ((l_area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING || l_area->testimonyRecording() == AreaData::TestimonyRecording::ADD) && !l_message.message.isEmpty())
  {
    // -1 indicates title
    if (l_area->statement() == -1)
    {
      l_message.message = "~~-- " + l_message.message + " --";
      l_message.textColor = 3;

      theory::SplashPacket l_splash;
      l_splash.type = theory::SplashType::WitnessTestimony;
      server->broadcastToArea(l_splash, areaId());
    }
    addStatement(l_message);
  }
  else if (l_area->testimonyRecording() == AreaData::TestimonyRecording::UPDATE)
  {
    l_message = updateStatement(l_message);
  }
  else if (l_area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK)
  {
    AreaData::TestimonyProgress l_progress;

    if (l_message.message == ">")
    {
      auto l_statement = l_area->jumpToStatement(l_area->statement() + 1);
      l_message = l_statement.first;
      l_progress = l_statement.second;
      m_pos = l_message.side;

      sendServerMessageArea(client_name + " moved to the next statement.");

      if (l_progress == AreaData::TestimonyProgress::LOOPED)
      {
        sendServerMessageArea("Last statement reached. Looping to first statement.");
      }
    }
    if (l_message.message == "<")
    {
      auto l_statement = l_area->jumpToStatement(l_area->statement() - 1);
      l_message = l_statement.first;
      l_progress = l_statement.second;
      m_pos = l_message.side;

      sendServerMessageArea(client_name + " moved to the previous statement.");

      if (l_progress == AreaData::TestimonyProgress::STAYED_AT_FIRST)
      {
        sendServerMessage("First statement reached.");
      }
    }
    if (l_message.message == "=")
    {
      auto l_statement = l_area->jumpToStatement(l_area->statement());
      l_message = l_statement.first;
      l_progress = l_statement.second;
      m_pos = l_message.side;

      sendServerMessageArea(client_name + " repeated the current statement.");
    }

    QRegularExpressionMatch jump_match = isTestimonyJumpCommand(l_message.message);
    if (jump_match.hasMatch())
    {
      int jump_idx = jump_match.captured("int").toInt();
      auto l_statement = l_area->jumpToStatement(jump_idx);
      l_message = l_statement.first;
      l_progress = l_statement.second;
      m_pos = l_message.side;

      sendServerMessageArea(client_name + " jumped to statement number " + QString::number(jump_idx) + ".");

      switch (l_progress)
      {
      case AreaData::TestimonyProgress::LOOPED:
        {
          sendServerMessageArea("Last statement reached. Looping to first statement.");
          break;
        }
      case AreaData::TestimonyProgress::STAYED_AT_FIRST:
        {
          sendServerMessage("First statement reached.");
          Q_FALLTHROUGH();
        }
      case AreaData::TestimonyProgress::OK:
      default:
        // No need to handle.
        break;
      }
    }

    if (l_message.character == theory::NoCharacterId)
    {
      return std::nullopt;
    }
  }

  return l_message;
}
