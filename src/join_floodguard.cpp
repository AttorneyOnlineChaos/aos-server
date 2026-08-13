#include "join_floodguard.h"

kenji::JoinFloodguard::JoinFloodguard(int burst, qint64 interval)
{
  setLimit(burst, interval);
}

int kenji::JoinFloodguard::burst() const
{
  return _burst;
}

qint64 kenji::JoinFloodguard::interval() const
{
  return _interval;
}

void kenji::JoinFloodguard::setLimit(int burst, qint64 interval)
{
  burst = qMax(1, burst);
  interval = qMax(1, interval);
  if (burst == _burst && interval == _interval)
  {
    return;
  }

  _burst = burst;
  _interval = interval;
  for (Entry &entry : _entries)
  {
    entry.limiter.setLimit(_burst, _interval);
  }
  if (_timer.isValid())
  {
    _timer.restart();
  }
  else
  {
    _timer.start();
  }
}

bool kenji::JoinFloodguard::allow(const QString &key, double amount)
{
  if (key.isEmpty())
  {
    return false;
  }

  if (amount <= 0.0)
  {
    return true;
  }

  cleanup();
  auto it = _entries.find(key);
  if (it == _entries.end())
  {
    it = _entries.insert(key, Entry{theory::RateLimiter{_burst, _interval}});
  }

  it->timestamp = 0;
  return it->limiter.allow(amount);
}

int kenji::JoinFloodguard::count() const
{
  return _entries.size();
}

void kenji::JoinFloodguard::clear()
{
  _entries.clear();
}

void kenji::JoinFloodguard::cleanup()
{
  const qint64 elapsed = _timer.restart();
  for (auto it = _entries.begin(); it != _entries.end();)
  {
    if (it->timestamp + elapsed >= _interval)
    {
      it = _entries.erase(it);
    }
    else
    {
      it->timestamp += elapsed;
      ++it;
    }
  }
}
