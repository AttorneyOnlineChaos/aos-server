#pragma once

#include "aoclient.h"

#include "game/game_defs.h"
#include "network/packet.h"
#include "protocol/packets/chat_packets.h"

#include <QString>

#include <functional>

namespace kenji
{
class Broadcaster
{
public:
  virtual ~Broadcaster() = default;

  virtual void broadcast(const theory::Packet &packet) = 0;
  virtual void broadcastIf(const theory::Packet &packet, const std::function<bool(const AOClient &)> &condition) = 0;
  virtual void broadcastToArea(const theory::Packet &packet, theory::AreaId areaId) = 0;
  virtual void broadcastToPlayer(const theory::Packet &packet, theory::ClientId playerId) = 0;

  virtual void broadcastMessage(const QString &message, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) = 0;
  virtual void broadcastMessageIf(const QString &message, const std::function<bool(const AOClient &)> &condition, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) = 0;
  virtual void broadcastMessageToArea(const QString &message, theory::AreaId areaId, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) = 0;
  virtual void broadcastMessageToPlayer(const QString &message, theory::ClientId playerId, theory::ServerMessagePacket::Level level = theory::ServerMessagePacket::Message) = 0;
};
} // namespace kenji
