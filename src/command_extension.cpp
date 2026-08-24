#include "command_extension.h"

#include "core/logging.h"
#include "kenji_log.h"

#include <QDebug>
#include <QSettings>

kenji::CommandExtension::CommandExtension()
{}

kenji::CommandExtension::CommandExtension(const QString &f_command_name)
{
  setCommandName(f_command_name);
}

kenji::CommandExtension::~CommandExtension()
{}

QString kenji::CommandExtension::getCommandName() const
{
  return m_command_name;
}

void kenji::CommandExtension::setCommandName(const QString &f_command_name)
{
  m_command_name = f_command_name;
  updateMergedAliases();
}

bool kenji::CommandExtension::checkCommandNameAndAlias(const QString &f_alias) const
{
  return m_merged_aliases.contains(f_alias, Qt::CaseInsensitive);
}

QStringList kenji::CommandExtension::getAliases() const
{
  return m_aliases;
}

QString kenji::CommandExtension::getDisplayName() const
{
  if (m_aliases.isEmpty())
  {
    return m_command_name;
  }

  const QString l_label = m_aliases.size() == 1 ? "alias" : "aliases";
  return m_command_name + " (" + l_label + ": " + m_aliases.join(", ") + ")";
}

void kenji::CommandExtension::setAliases(const QStringList &f_aliases)
{
  m_aliases = f_aliases;
  for (QString &i_alias : m_aliases)
  {
    i_alias = i_alias.toLower();
  }
  updateMergedAliases();
}

QList<kenji::ACLRole::Permission> kenji::CommandExtension::getPermissions(const QList<ACLRole::Permission> &f_defaultPermissions) const
{
  return m_permissions.isEmpty() ? f_defaultPermissions : m_permissions;
}

QList<kenji::ACLRole::Permission> kenji::CommandExtension::getPermissions() const
{
  return getPermissions(QList<ACLRole::Permission>{});
}

void kenji::CommandExtension::setPermissions(const QList<ACLRole::Permission> &f_permissions)
{
  m_permissions = f_permissions;
}

void kenji::CommandExtension::setPermissionsByCaption(const QStringList &f_captions)
{
  QList<ACLRole::Permission> l_permissions;
  const QStringList l_permission_captions = ACLRole::PERMISSION_CAPTIONS.values();
  for (const QString &i_caption : qAsConst(f_captions))
  {
    const QString l_lower_caption = i_caption.toLower();
    if (!l_permission_captions.contains(l_lower_caption))
    {
      zWarning(log::commands) << "error: permission" << i_caption << "does not exist";
      continue;
    }
    l_permissions.append(ACLRole::PERMISSION_CAPTIONS.key(l_lower_caption));
  }
  setPermissions(l_permissions);
}

void kenji::CommandExtension::updateMergedAliases()
{
  m_merged_aliases = QStringList{m_command_name} + m_aliases;
}

kenji::CommandExtensionCollection::CommandExtensionCollection(QObject *parent)
    : QObject(parent)
{}

kenji::CommandExtensionCollection::~CommandExtensionCollection()
{}

void kenji::CommandExtensionCollection::setCommandNameWhitelist(const QStringList &f_command_names)
{
  m_command_name_whitelist = f_command_names;
  for (QString &i_alias : m_command_name_whitelist)
  {
    i_alias = i_alias.toLower();
  }
}

QList<kenji::CommandExtension> kenji::CommandExtensionCollection::getExtensions() const
{
  return m_extensions.values();
}

bool kenji::CommandExtensionCollection::containsExtension(const QString &f_command_name) const
{
  if (m_extensions.contains(f_command_name))
  {
    return true;
  }

  for (const CommandExtension &i_extension : m_extensions)
  {
    if (i_extension.checkCommandNameAndAlias(f_command_name))
    {
      return true;
    }
  }
  return false;
}

kenji::CommandExtension kenji::CommandExtensionCollection::getExtension(const QString &f_command_name) const
{
  if (m_extensions.contains(f_command_name))
  {
    return m_extensions.value(f_command_name);
  }

  for (const CommandExtension &i_extension : m_extensions)
  {
    if (i_extension.checkCommandNameAndAlias(f_command_name))
    {
      return i_extension;
    }
  }
  return CommandExtension();
}

bool kenji::CommandExtensionCollection::loadFile(const QString &f_filename)
{
  QSettings l_settings(f_filename, QSettings::IniFormat);
  if (l_settings.status() != QSettings::NoError)
  {
    zWarning(log::commands) << "error: failed to load file" << f_filename << "; aborting";
    return false;
  }

  m_extensions.clear();
  QStringList l_alias_records;
  QStringList l_command_records;
  const QStringList l_group_list = l_settings.childGroups();
  for (const QString &i_group : l_group_list)
  {
    const QString l_command_name = i_group.toLower();
    if (!m_command_name_whitelist.isEmpty() && !m_command_name_whitelist.contains(l_command_name))
    {
      zWarning(log::commands) << "error: command" << l_command_name << "cannot be extended; does not exist";
      continue;
    }

    if (l_command_records.contains(l_command_name))
    {
      zWarning(log::commands) << "warning: command extension" << l_command_name << "already exist";
      continue;
    }
    l_command_records.append(l_command_name);

    l_settings.beginGroup(i_group);

    QStringList l_aliases = l_settings.value("aliases").toString().split(" ", Qt::SkipEmptyParts);
    for (QString &i_alias : l_aliases)
    {
      i_alias = i_alias.toLower();
    }

    for (const QString &i_recorded_alias : l_alias_records)
    {
      if (l_aliases.contains(i_recorded_alias))
      {
        zWarning(log::commands) << "warning: command alias" << i_recorded_alias << "was already defined";
        l_aliases.removeAll(i_recorded_alias);
      }
    }
    l_alias_records.append(l_aliases);

    CommandExtension l_extension(l_command_name);
    l_extension.setAliases(l_aliases);
    l_extension.setPermissionsByCaption(l_settings.value("permissions").toString().split(" ", Qt::SkipEmptyParts));
    m_extensions.insert(l_command_name, std::move(l_extension));

    l_settings.endGroup();
  }

  return true;
}
