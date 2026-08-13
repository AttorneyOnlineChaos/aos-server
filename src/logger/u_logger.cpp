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

void kenji::ULogger::logIC(const QString &f_area_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name, const QString &f_message)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ic") + "\n").arg(l_time, f_area_name, f_ipid, f_id, f_char_name, f_ooc_name, f_message);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logMusic(const QString &f_char_name, const QString &f_ooc_name, const QString &f_ipid, const QString &f_area_name, const QString &f_track)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("music") + "\n").arg(l_time, f_char_name, f_ooc_name, f_ipid, f_area_name, f_track);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logOOC(const QString &f_area_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name, const QString &f_message)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ooc") + "\n").arg(l_time, f_area_name, f_ipid, f_id, f_char_name, f_ooc_name, f_message);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logLogin(const QString &f_char_name, const QString &f_ooc_name, const QString &f_moderator_name, const QString &f_ipid, const QString &f_area_name, const bool &f_success)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_success = f_success ? "SUCCESS][" + f_moderator_name : "FAILED][" + f_moderator_name;
  QString l_logEntry = QString(m_logtext.value("login") + "\n").arg(l_time, l_success, f_ipid, f_char_name, f_ooc_name);
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logCMD(const QString &f_char_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_command, const QStringList &f_args, const QString &f_area_name)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry;
  // Some commands contain sensitive data, like passwords
  // These must be filtered out
  if (f_command == "login")
  {
    l_logEntry = QString(m_logtext.value("cmdlogin") + "\n").arg(l_time, f_area_name, f_char_name, f_ooc_name, f_ipid);
  }
  else if (f_command == "rootpass")
  {
    l_logEntry = QString(m_logtext.value("cmdrootpass") + "\n").arg(l_time, f_area_name, f_char_name, f_ooc_name, f_ipid);
  }
  else if (f_command == "adduser" && !f_args.isEmpty())
  {
    l_logEntry = QString(m_logtext.value("adduser") + "\n").arg(l_time, f_area_name, f_char_name, f_ooc_name, f_args.at(0), f_ipid);
  }
  else
  {
    l_logEntry = QString(m_logtext.value("cmd") + "\n").arg(l_time, f_area_name, f_char_name, f_ooc_name, f_command, f_args.join(" "), f_ipid);
  }
  updateAreaBuffer(f_area_name, l_logEntry);
}

void kenji::ULogger::logKick(const QString &f_moderator, const QString &f_target_ipid)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("kick") + "\n").arg(l_time, f_moderator, f_target_ipid);
  updateAreaBuffer("SERVER", l_logEntry);
}

void kenji::ULogger::logBan(const QString &f_moderator, const QString &f_target_ipid, const QString &f_duration)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("ban") + "\n").arg(l_time, f_moderator, f_target_ipid, f_duration);
  updateAreaBuffer("SERVER", l_logEntry);
}

void kenji::ULogger::logModcall(const QString &f_area_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEvent = QString(m_logtext.value("modcall") + "\n").arg(l_time, f_area_name, f_ipid, f_id, f_char_name, f_ooc_name);
  updateAreaBuffer(f_area_name, l_logEvent);

  if (ConfigManager::loggingType() == DataTypes::LogType::MODCALL)
  {
    writerModcall->flush(f_area_name, buffer(f_area_name));
  }
}

void kenji::ULogger::logConnectionAttempt(const QString &f_ip_address, const QString &f_ipid, const QString &f_hwid)
{
  QString l_time = QDateTime::currentDateTime().toString("ddd MMMM d yyyy | hh:mm:ss");
  QString l_logEntry = QString(m_logtext.value("connect") + "\n").arg(l_time, f_ip_address, f_ipid, f_hwid);
  updateAreaBuffer("SERVER", l_logEntry);
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
