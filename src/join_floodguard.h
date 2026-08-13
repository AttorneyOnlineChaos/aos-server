#pragma once

#include "core/rate_limiter.h"

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QtGlobal>

namespace kenji
{
class JoinFloodguard
{
public:
  JoinFloodguard(int burst = 100, qint64 interval = 3600000);

  int burst() const;
  qint64 interval() const;
  void setLimit(int burst, qint64 interval);

  bool allow(const QString &key, double amount = 1.0);

  int count() const;
  void clear();

private:
  struct Entry
  {
    theory::RateLimiter limiter;
    qint64 timestamp = 0;
  };

  int _burst = 0;
  qint64 _interval = 0;

  QElapsedTimer _timer;
  QHash<QString, Entry> _entries;

  void cleanup();
};
} // namespace kenji
