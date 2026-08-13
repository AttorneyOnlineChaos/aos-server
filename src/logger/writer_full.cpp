#include "logger/writer_full.h"

kenji::WriterFull::WriterFull(QObject *parent)
    : QObject(parent)
{
  l_dir.setPath("logs/");
  if (!l_dir.exists())
  {
    l_dir.mkpath(".");
  }
}

void kenji::WriterFull::flush(const QString &f_entry)
{
  l_logfile.setFileName(QString("logs/%1.log").arg(QDate::currentDate().toString("yyyy-MM-dd")));

  if (l_logfile.open(QIODevice::WriteOnly | QIODevice::Append))
  {
    QTextStream file_stream(&l_logfile);
    file_stream.setGenerateByteOrderMark(true);
    file_stream << f_entry;
  }
  l_logfile.close();
}

void kenji::WriterFull::flush(const QString &f_entry, const QString &f_area_name)
{
  l_logfile.setFileName(QString("logs/%1_%2.log").arg(f_area_name, QDate::currentDate().toString("yyyy-MM-dd")));

  if (l_logfile.open(QIODevice::WriteOnly | QIODevice::Append))
  {
    QTextStream file_stream(&l_logfile);
    file_stream.setGenerateByteOrderMark(true);
    file_stream << f_entry;
  }
  l_logfile.close();
};
