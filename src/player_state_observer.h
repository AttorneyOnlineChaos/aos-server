#pragma once

#include "ao_client.h"

#include "protocol/packets/roster_packets.h"

#include <QList>
#include <QObject>
#include <QString>

namespace kenji
{
class PlayerStateObserver : public QObject
{
  Q_OBJECT

public:
  explicit PlayerStateObserver(QObject *parent = nullptr);
  virtual ~PlayerStateObserver();

  void registerClient(AOClient *client);
  void unregisterClient(AOClient *client);

private:
  QList<AOClient *> m_client_list;

  static theory::PlayerUpdatePacket playerUpdate(const AOClient &client, theory::PlayerUpdatePacket::Property property);

  void broadcast(const theory::Packet &packet);
  void shipRoster(AOClient *target);
};
} // namespace kenji
