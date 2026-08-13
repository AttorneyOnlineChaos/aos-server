#pragma once

#include <QLoggingCategory>

namespace kenji
{
namespace log
{
Q_DECLARE_LOGGING_CATEGORY(main)
Q_DECLARE_LOGGING_CATEGORY(config)
Q_DECLARE_LOGGING_CATEGORY(master)
Q_DECLARE_LOGGING_CATEGORY(database)
Q_DECLARE_LOGGING_CATEGORY(commands)
Q_DECLARE_LOGGING_CATEGORY(network)
Q_DECLARE_LOGGING_CATEGORY(protocol)
Q_DECLARE_LOGGING_CATEGORY(discord)
Q_DECLARE_LOGGING_CATEGORY(acl)
} // namespace log
} // namespace kenji
