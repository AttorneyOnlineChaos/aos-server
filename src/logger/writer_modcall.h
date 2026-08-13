#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QQueue>
#include <QTextStream>

namespace kenji
{
/**
 * @brief A class to handle file interaction when writing the modcall buffer.
 */
class WriterModcall : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief Constructor for modcall logwriter
   *
   * @param QObject pointer to the parent object.
   */
  WriterModcall(QObject *parent = nullptr);
  ;

  /**
   * @brief Deconstructor for modcall logwriter.
   *
   * @details Doesn't really do anything, but its here for completeness sake.
   */
  virtual ~WriterModcall() {}

  /**
   * @brief Function to write area buffer into a logfile.
   * @param QQueue of the area that will be written into the logfile.
   * @param Name of the area for the filename.
   */
  void flush(const QString &f_area_name, QQueue<QString> f_buffer);

private:
  /**
   * @brief Filename of the logfile used.
   */
  QFile l_logfile;

  /**
   * @brief Directory where logfiles will be stored.
   */
  QDir l_dir;
};
} // namespace kenji
