#include "timer.h"

#include "core/json_codec.h"

kenji::Timer::Timer(theory::TimerId id, QObject *parent)
    : QObject{parent}
    , _id{id}
{
  _timer.setSingleShot(true);

  connect(&_timer, &QTimer::timeout, this, &Timer::handleTimeout);
}

theory::TimerId kenji::Timer::id() const
{
  return _id;
}

theory::TimerState kenji::Timer::state() const
{
  return _state;
}

void kenji::Timer::start()
{
  if (_state == theory::TimerState::Running)
  {
    return;
  }

  if (_durationMs == 0)
  {
    handleTimeout();
    return;
  }

  _timer.setInterval(_durationMs);
  _timer.start();
  setState(theory::TimerState::Running);
}

void kenji::Timer::pause(bool enabled)
{
  if (enabled)
  {
    if (_state != theory::TimerState::Running)
    {
      return;
    }

    _remainingMs = _timer.remainingTime();
    _timer.stop();
    setState(theory::TimerState::Paused);
    return;
  }
  else
  {
    if (_state != theory::TimerState::Paused)
    {
      return;
    }

    _timer.setInterval(_remainingMs);
    _timer.start();
    setState(theory::TimerState::Running);
  }
}

void kenji::Timer::stop()
{
  if (_state == theory::TimerState::NotRunning)
  {
    return;
  }

  _timer.stop();
  _remainingMs = 0;
  setState(theory::TimerState::NotRunning);
}

qint64 kenji::Timer::durationMs() const
{
  return _durationMs;
}

qint64 kenji::Timer::remainingMs() const
{
  if (_state == theory::TimerState::Running)
  {
    return qMax<qint64>(0, _timer.remainingTime());
  }

  if (_state == theory::TimerState::Paused)
  {
    return _remainingMs;
  }

  return 0;
}

void kenji::Timer::setDurationMs(qint64 durationMs)
{
  _durationMs = qMax<qint64>(0, durationMs);
  _timer.setInterval(_durationMs);
}

bool kenji::Timer::isVisible() const
{
  return _visible;
}

void kenji::Timer::setVisible(bool visible)
{
  if (_visible == visible)
  {
    return;
  }

  _visible = visible;
  Q_EMIT visibilityChanged(_visible);
}

void kenji::Timer::setState(theory::TimerState state)
{
  if (_state == state)
  {
    return;
  }

  _state = state;
  Q_EMIT stateChanged(_state);
}

void kenji::Timer::handleTimeout()
{
  _state = theory::TimerState::NotRunning;
  Q_EMIT stateChanged(_state);
  Q_EMIT timeout();
}

theory::TimerPacket kenji::makeTimerPacket(const Timer &timer, theory::TimerPacket::Property property)
{
  theory::TimerPacket packet;
  packet.timerId = timer.id();
  packet.property = property;

  switch (property)
  {
  default:
  case theory::TimerPacket::NoProperty:
    break;
  case theory::TimerPacket::State:
    packet.data = theory::encodeJson(timer.state());
    break;
  case theory::TimerPacket::Tick:
    packet.data = theory::encodeJson(timer.remainingMs());
    break;
  case theory::TimerPacket::Visibility:
    packet.data = theory::encodeJson(timer.isVisible());
    break;
  }

  return packet;
}
