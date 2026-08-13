#include "kenji_info.h"

QString kenji::softwareName()
{
  return QStringLiteral("kenji");
}

QVersionNumber kenji::softwareVersion()
{
  return QVersionNumber{1, 9};
}
