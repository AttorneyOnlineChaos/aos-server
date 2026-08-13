#pragma once

#include "game/game_defs.h"
#include "protocol/packets/area_packets.h"

#include <QObject>
#include <QTimer>

namespace kenji
{
class Timer : public QObject
{
  Q_OBJECT

public:
  explicit Timer(theory::TimerId id, QObject *parent = nullptr);

  theory::TimerId id() const;

  theory::TimerState state() const;
  void start();
  void stop();
  void pause(bool enabled);

  qint64 duration() const;
  void setDuration(qint64 milliseconds);

  qint64 remaining() const;

  bool isVisible() const;
  void setVisible(bool visible);

Q_SIGNALS:
  void stateChanged(theory::TimerState state);
  void visibilityChanged(bool visible);
  void timeout();

private:
  theory::TimerId _id;
  theory::TimerState _state = theory::TimerState::NotRunning;
  qint64 _duration = 0;
  qint64 _remaining = 0;
  bool _visible = false;
  QTimer _timer;

  void setState(theory::TimerState state);

private Q_SLOTS:
  void handleTimeout();
};

theory::TimerPacket makeTimerPacket(const Timer &timer, theory::TimerPacket::Property property);
} // namespace kenji
