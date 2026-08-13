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

  if (_duration == 0)
  {
    handleTimeout();
    return;
  }

  _timer.setInterval(_duration);
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
    _remaining = _timer.remainingTime();
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
    _timer.setInterval(_remaining);
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
  _remaining = 0;
  setState(theory::TimerState::NotRunning);
}

qint64 kenji::Timer::duration() const
{
  return _duration;
}

qint64 kenji::Timer::remaining() const
{
  if (_state == theory::TimerState::Running)
  {
    return qMax<qint64>(0, _timer.remainingTime());
  }

  if (_state == theory::TimerState::Paused)
  {
    return _remaining;
  }

  return 0;
}

void kenji::Timer::setDuration(qint64 milliseconds)
{
  _duration = qMax<qint64>(0, milliseconds);
  _timer.setInterval(_duration);
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
    packet.data = theory::encodeJson(timer.remaining());
    break;
  case theory::TimerPacket::Visibility:
    packet.data = theory::encodeJson(timer.isVisible());
    break;
  }

  return packet;
}
