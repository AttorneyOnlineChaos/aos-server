#include "ao_client.h"

#include "area_data.h"
#include "config_manager.h"
#include "server.h"

void kenji::AOClient::addStatement(theory::IcMessagePacket message)
{
  if (checkTestimonySymbols(message.message))
  {
    return;
  }
  AreaData *area = server->getAreaById(areaId());
  int c_statement = area->statement();
  if (c_statement >= -1)
  {
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING)
    {
      if (c_statement <= ConfigManager::maxStatements())
      {
        if (c_statement == -1)
        {
          message.textColor = 3;
        }
        else
        {
          message.textColor = 1;
        }
        area->recordStatement(message);
        return;
      }
      else
      {
        sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
      }
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::ADD)
    {
      message.textColor = 1;
      if (c_statement == 0)
      {
        area->addStatement(c_statement, message);
      }
      area->addStatement(c_statement + 1, message);
      area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
    }
    else
    {
      sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
      area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
    }
  }
}

theory::IcMessagePacket kenji::AOClient::updateStatement(theory::IcMessagePacket message)
{
  if (checkTestimonySymbols(message.message))
  {
    return message;
  }
  AreaData *area = server->getAreaById(areaId());
  int c_statement = area->statement();
  area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
  if (c_statement <= 0 || c_statement >= area->testimony().size() || area->testimony()[c_statement].message.isEmpty())
  {
    sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
  }
  else
  {
    message.textColor = 1;
    area->replaceStatement(c_statement, message);
    sendServerMessage("Updated current statement.");
    return area->testimony()[c_statement];
  }
  return message;
}

void kenji::AOClient::clearTestimony()
{
  AreaData *area = server->getAreaById(areaId());
  area->clearTestimony();
}

bool kenji::AOClient::checkTestimonySymbols(const QString &message)
{
  if (message.contains('>') || message.contains('<'))
  {
    sendServerMessage("Unable to add statements containing '>' or '<'.");
    return true;
  }
  return false;
}
