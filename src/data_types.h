#pragma once

#include <QDebug>
#include <QVariant>

namespace kenji
{
/**
 * @brief A class for handling several custom data types.
 */
class DataTypes
{
  Q_GADGET

public:
  /**
   * @brief Custom type for authorization types.
   */
  enum class AuthType
  {
    SIMPLE,
    ADVANCED
  };
  Q_ENUM(AuthType);

  /**
   * @brief Custom type for logging types.
   */
  enum class LogType
  {
    MODCALL,
    FULL,
    FULLAREA
  };
  Q_ENUM(LogType)
};

template <typename T>
T toDataType(const QString &f_string)
{
  return QVariant(f_string).value<T>();
}

template <typename T>
QString fromDataType(const T &f_t)
{
  return QVariant::fromValue(f_t).toString();
}
} // namespace kenji
