#include "logger/u_logger.h"

kenji::ULogger::ULogger(QObject *parent)
    : QObject(parent)
{
  switch (ConfigManager::loggingType())
  {
  case DataTypes::LogType::MODCALL:
    writerModcall = new WriterModcall;
    break;
  case DataTypes::LogType::FULL:
  case DataTypes::LogType::FULLAREA:
    writerFull = new WriterFull;
    break;
  }

  loadLogtext();
}

kenji::ULogger::~ULogger()
{
  delete writerModcall;
  delete writerFull;
}

QString kenji::ULogger::logIdentity(theory::UserId id)
{
  return id == theory::NoUserId ? QStringLiteral("guest") : QString::number(id);
}

void kenji::ULogger::logIC(const QString &f_area_name, theory::UserId f_user_id, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name, const QString &f_message)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ic") + "\n").arg(l_time, f_area_name, logIdentity(f_user_id), f_id, f_char_name, f_ooc_name, f_message);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logMusic(const QString &f_char_name, const QString &f_ooc_name, theory::UserId f_user_id, const QString &f_area_name, const QString &f_track)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("music") + "\n").arg(l_time, f_char_name, f_ooc_name, logIdentity(f_user_id), f_area_name, f_track);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logOOC(const QString &f_area_name, theory::UserId f_user_id, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name, const QString &f_message)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ooc") + "\n").arg(l_time, f_area_name, logIdentity(f_user_id), f_id, f_char_name, f_ooc_name, f_message);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logCMD(const QString &f_char_name, theory::UserId f_user_id, const QString &f_ooc_name, const QString &f_command, const QStringList &f_args, const QString &f_area_name)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("cmd") + "\n").arg(l_time, f_area_name, f_char_name, f_ooc_name, f_command, f_args.join(" "), logIdentity(f_user_id));
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logKick(theory::UserId f_moderator, theory::UserId f_target)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("kick") + "\n").arg(l_time, logIdentity(f_moderator), logIdentity(f_target));
  updateAreaBuffer("SERVER", l_logEntry);
}

void kenji::ULogger::logBan(theory::UserId f_moderator, theory::UserId f_target, const QString &f_duration)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ban") + "\n").arg(l_time, logIdentity(f_moderator), logIdentity(f_target), f_duration);
  updateAreaBuffer("SERVER", l_logEntry);
}

void kenji::ULogger::logModcall(const QString &f_area_name, theory::UserId f_user_id, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEvent = QString(m_logtext.value("modcall") + "\n").arg(l_time, f_area_name, logIdentity(f_user_id), f_id, f_char_name, f_ooc_name);
  updateAreaBuffer(f_area_name, l_logEvent);

  if (ConfigManager::loggingType() == DataTypes::LogType::MODCALL)
  {
    writerModcall->flush(f_area_name, buffer(f_area_name));
  }
}

void kenji::ULogger::loadLogtext()
{
  // All of this to prevent one single clazy warning from appearing.
  for (auto iterator = m_logtext.keyBegin(), end = m_logtext.keyEnd(); iterator != end; ++iterator)
  {
    QString l_tempstring = ConfigManager::LogText(iterator.operator*());
    if (!l_tempstring.isEmpty())
    {
      m_logtext[iterator.operator*()] = l_tempstring;
    }
  }
}

void kenji::ULogger::updateAreaBuffer(const QString &f_area_name, const QString &f_log_entry)
{
  QQueue<QString> l_buffer = m_bufferMap.value(f_area_name);

  if (l_buffer.length() <= ConfigManager::logBuffer())
  {
    l_buffer.enqueue(f_log_entry);
  }
  else
  {
    l_buffer.dequeue();
    l_buffer.enqueue(f_log_entry);
  }

  m_bufferMap.insert(f_area_name, l_buffer);

  if (ConfigManager::loggingType() == DataTypes::LogType::FULL)
  {
    writerFull->flush(f_log_entry);
  }

  if (ConfigManager::loggingType() == DataTypes::LogType::FULLAREA)
  {
    writerFull->flush(f_log_entry, f_area_name);
  }
}

QQueue<QString> kenji::ULogger::buffer(const QString &f_area_name)
{
  return m_bufferMap.value(f_area_name);
}
