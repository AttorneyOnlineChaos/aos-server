#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QTextStream>

namespace kenji
{
/**
 * @brief A class to handle file interaction when writing in full log mode.
 */
class WriterFull : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief Constructor for full logwriter
   *
   * @param QObject pointer to the parent object.
   */
  WriterFull(QObject *parent = nullptr);
  ;

  /**
   * @brief Deconstructor for full logwriter.
   *
   * @details Doesn't really do anything, but its here for completeness sake.
   */
  virtual ~WriterFull() {}

  /**
   * @brief Function to write log entry into a logfile.
   * @param Preformatted QString which will be written into the logfile.
   */
  void flush(const QString &f_entry);

  /**
   * @brief Writes log entry into area seperated logfiles.
   * @param Preformatted QString which will be written into the logfile
   * @param Area name of the target logfile.
   */
  void flush(const QString &f_entry, const QString &f_area_name);

private:
  /**
   * @brief Filename of the logfile used. This will always be the time the server starts up.
   */
  QFile l_logfile;

  /**
   * @brief Directory where logfiles will be stored.
   */
  QDir l_dir;
};
} // namespace kenji
