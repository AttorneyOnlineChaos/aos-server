#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace kenji
{
class CommandParser
{
public:
  static std::optional<QStringList> parseCommand(const QString &text);
};
} // namespace kenji
