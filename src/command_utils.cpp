#include "command_utils.h"

#include <QChar>

std::optional<QStringList> kenji::CommandParser::parseCommand(const QString &text)
{
  QStringList tokens;
  QString token;
  bool quoted = false;
  bool open = false;

  for (int i = 0; i < text.size(); ++i)
  {
    const QChar current = text.at(i);

    if (current == '\\')
    {
      if (i + 1 >= text.size())
      {
        return std::nullopt;
      }
      ++i;
      token.append(text.at(i));
      open = true;
      continue;
    }

    if (current == '"')
    {
      if (quoted)
      {
        if (i + 1 < text.size() && text.at(i + 1) != ' ')
        {
          return std::nullopt;
        }
        quoted = false;
        continue;
      }

      if (open)
      {
        return std::nullopt;
      }
      quoted = true;
      open = true;
      continue;
    }

    if (current == ' ' && !quoted)
    {
      if (open)
      {
        tokens.append(token);
        token.clear();
        open = false;
      }
      continue;
    }

    token.append(current);
    open = true;
  }

  if (quoted)
  {
    return std::nullopt;
  }

  if (open)
  {
    tokens.append(token);
  }
  return tokens;
}
