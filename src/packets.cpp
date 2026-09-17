#include "ao_client.h"

#include "area_data.h"
#include "music_manager.h"

#include <QQueue>

void kenji::AOClient::shipGameError(const theory::GameError &error)
{
  theory::GameErrorPacket l_packet;
  l_packet.error = error;
  shipPacket(l_packet);
}

QString kenji::AOClient::dezalgo(QString p_text)
{
  QRegularExpression rxp("([̴̵̶̷̸̡̢̧̨̛̖̗̘̙̜̝̞̟̠̣̤̥̦̩̪̫̬̭̮̯̰̱̲̳̹̺̻̼͇͈͉͍͎̀́̂̃̄̅̆̇̈̉̊̋̌̍̎̏̐̑̒̓̔̽̾̿̀́͂̓̈́͆͊͋͌̕̚ͅ͏͓͔͕͖͙͚͐͑͒͗͛ͣͤͥͦͧͨͩͪͫͬͭͮͯ͘͜͟͢͝͞͠͡])");
  QString filtered = p_text.replace(rxp, "");
  return filtered;
}

void kenji::AOClient::updateJudgeLog(AreaData *area, AOClient *client, const QString &action)
{
  QString l_timestamp = QTime::currentTime().toString("hh:mm:ss");
  QString l_uid = QString::number(client->id);
  QString l_char_name = client->character().toString();
  QString l_message = action;
  QString l_logmessage = QString("[%1]: [%2] %3 %4").arg(l_timestamp, l_uid, l_char_name, l_message);
  area->appendJudgelog(l_logmessage);
}
