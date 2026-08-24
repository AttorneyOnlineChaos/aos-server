#include "db_manager.h"

#include "core/logging.h"
#include "kenji_log.h"

#include <QStringList>

QString kenji::BanInfo::until() const
{
  return QDateTime::fromSecsSinceEpoch(time).addSecs(duration).toString("MM/dd/yyyy, hh:mm");
}

QString kenji::BanInfo::toString() const
{
  QStringList report;
  report << "Ban ID: " + QString::number(id);
  report << "Affected IPID: " + ipid;
  report << "Affected HDID: " + hdid;
  report << "Reason for ban: " + reason;
  report << "Date of ban: " + QDateTime::fromSecsSinceEpoch(time).toString("MM/dd/yyyy, hh:mm");
  report << "Ban lasts until: " + until();
  report << "Moderator: " + moderator;
  report << "Revoked: " + QString(revoked ? "yes" : "no");

  return report.join("\n");
}

kenji::DBManager::DBManager(QObject *parent)
    : QObject{parent}
    , DRIVER("QSQLITE")
{
  const QString db_filename = "config/kenji.db";
  QFileInfo db_info(db_filename);
  if (!db_info.exists())
  {
    zWarning(log::database) << tr("Database Info: Database not found. Attempting to create new database.");
  }
  else
  {
    // We should only check if a file is readable/writeable when it actually exists.
    if (!db_info.isReadable() || !db_info.isWritable())
    {
      zCritical(log::database) << tr("Database Error: Missing permissions. Check if \"%1\" is writable.").arg(db_filename);
    }
  }

  db = QSqlDatabase::addDatabase(DRIVER);
  db.setDatabaseName("config/kenji.db");
  if (!db.open())
  {
    zCritical(log::database) << "Database Error:" << db.lastError();
  }
  db_version = checkVersion();
  QSqlQuery create_ban_table("CREATE TABLE IF NOT EXISTS bans ('ID' INTEGER, 'IPID' TEXT, 'HDID' TEXT, 'IP' TEXT, 'TIME' INTEGER, 'REASON' TEXT, 'DURATION' INTEGER, 'MODERATOR' TEXT, 'REVOKED' INTEGER, PRIMARY KEY('ID' AUTOINCREMENT))");
  create_ban_table.exec();
  QSqlQuery create_user_table("CREATE TABLE IF NOT EXISTS users ('ID' INTEGER, 'USERNAME' TEXT, 'SALT' TEXT, 'PASSWORD' TEXT, 'ACL' TEXT, PRIMARY KEY('ID' AUTOINCREMENT))");
  create_user_table.exec();
  if (db_version != DB_VERSION)
  {
    updateDB(db_version);
  }
}

QPair<bool, kenji::BanInfo> kenji::DBManager::isIPBanned(const QString &ipid)
{
  QSqlQuery query;
  query.prepare("SELECT * FROM BANS WHERE IPID = ? ORDER BY TIME DESC");
  query.addBindValue(ipid);
  query.exec();
  BanInfo ban;
  if (query.first())
  {
    ban.id = query.value(0).toInt();
    ban.ipid = query.value(1).toString();
    ban.hdid = query.value(2).toString();
    ban.ip = QHostAddress(query.value(3).toString());
    ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
    ban.reason = query.value(5).toString();
    ban.duration = query.value(6).toLongLong();
    ban.moderator = query.value(7).toString();
    ban.revoked = query.value(8).toBool();
    if (ban.revoked)
    {
      return {false, ban};
    }
    unsigned long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    if (ban.time + ban.duration > current_time)
    {
      return {true, ban};
    }
    else
    {
      return {false, ban};
    }
  }
  else
  {
    return {false, ban};
  }
}

QPair<bool, kenji::BanInfo> kenji::DBManager::isHDIDBanned(const QString &hdid)
{
  QSqlQuery query;
  query.prepare("SELECT * FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
  query.addBindValue(hdid);
  query.exec();
  BanInfo ban;
  if (query.first())
  {
    ban.id = query.value(0).toInt();
    ban.ipid = query.value(1).toString();
    ban.hdid = query.value(2).toString();
    ban.ip = QHostAddress(query.value(3).toString());
    ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
    ban.reason = query.value(5).toString();
    ban.duration = query.value(6).toLongLong();
    ban.moderator = query.value(7).toString();
    ban.revoked = query.value(8).toBool();
    if (ban.revoked)
    {
      return {false, ban};
    }
    unsigned long current_time = QDateTime::currentDateTime().toSecsSinceEpoch();
    if (ban.time + ban.duration > current_time)
    {
      return {true, ban};
    }
    else
    {
      return {false, ban};
    }
  }
  else
  {
    return {false, ban};
  }
}

int kenji::DBManager::getBanID(const QString &hdid)
{
  QSqlQuery query;
  query.prepare("SELECT ID FROM BANS WHERE HDID = ? ORDER BY TIME DESC");
  query.addBindValue(hdid);
  query.exec();
  if (query.first())
  {
    return query.value(0).toInt();
  }
  else
  {
    return -1;
  }
}

int kenji::DBManager::getBanID(const QHostAddress &ip)
{
  QSqlQuery query;
  query.prepare("SELECT ID FROM BANS WHERE IP = ? ORDER BY TIME DESC");
  query.addBindValue(ip.toString());
  query.exec();
  if (query.first())
  {
    return query.value(0).toInt();
  }
  else
  {
    return -1;
  }
}

QList<kenji::BanInfo> kenji::DBManager::getRecentBans()
{
  QList<BanInfo> return_list;
  QSqlQuery query;
  query.prepare("SELECT * FROM BANS ORDER BY TIME DESC LIMIT 5");
  query.setForwardOnly(true);
  query.exec();
  while (query.next())
  {
    BanInfo ban;
    ban.id = query.value(0).toInt();
    ban.ipid = query.value(1).toString();
    ban.hdid = query.value(2).toString();
    ban.ip = QHostAddress(query.value(3).toString());
    ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
    ban.reason = query.value(5).toString();
    ban.duration = query.value(6).toLongLong();
    ban.moderator = query.value(7).toString();
    ban.revoked = query.value(8).toBool();
    return_list.append(ban);
  }
  std::reverse(return_list.begin(), return_list.end());
  return return_list;
}

void kenji::DBManager::addBan(const BanInfo &ban)
{
  QSqlQuery query;
  query.prepare("INSERT INTO BANS(IPID, HDID, IP, TIME, REASON, DURATION, MODERATOR, REVOKED) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
  query.addBindValue(ban.ipid);
  query.addBindValue(ban.hdid);
  query.addBindValue(ban.ip.toString());
  query.addBindValue(QString::number(ban.time));
  query.addBindValue(ban.reason);
  query.addBindValue(ban.duration);
  query.addBindValue(ban.moderator);
  query.addBindValue(ban.revoked);
  if (!query.exec())
  {
    zDebug(log::database) << "SQL Error:" << query.lastError().text();
  }
}

bool kenji::DBManager::invalidateBan(int id)
{
  QSqlQuery ban_exists;
  ban_exists.prepare("SELECT REVOKED FROM bans WHERE ID = ?");
  ban_exists.addBindValue(id);
  ban_exists.exec();

  if (!ban_exists.first())
  {
    return false;
  }

  QSqlQuery query;
  query.prepare("UPDATE bans SET REVOKED = 1 WHERE ID = ?");
  query.addBindValue(id);
  if (!query.exec())
  {
    zDebug(log::database) << "SQL Error:" << query.lastError().text();
    return false;
  }
  return true;
}

bool kenji::DBManager::createUser(const QString &f_username, const QByteArray &f_salt, const QString &f_password, const QString &f_acl)
{
  QSqlQuery username_exists;
  username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
  username_exists.addBindValue(f_username);
  username_exists.exec();

  if (username_exists.first())
  {
    return false;
  }

  QSqlQuery query;

  QString salted_password = CryptoHelper::hash_password(f_salt, f_password);

  query.prepare("INSERT INTO users(USERNAME, SALT, PASSWORD, ACL) VALUES(?, ?, ?, ?)");
  query.addBindValue(f_username);
  query.addBindValue(f_salt.toHex());
  query.addBindValue(salted_password);
  query.addBindValue(f_acl);
  query.exec();

  return true;
}

bool kenji::DBManager::deleteUser(const QString &username)
{
  if (username == "root")
  {
    // To prevent lockout scenarios where an admin may accidentally delete root.
    return false;
  }

  {
    QSqlQuery username_exists;
    username_exists.prepare("SELECT EXISTS(SELECT USERNAME FROM users WHERE USERNAME = ?)");
    username_exists.addBindValue(username);
    username_exists.exec();
    username_exists.first();
    // If EXISTS can't find a record, it returns 0.
    if (username_exists.value(0).toInt() == 0)
    {
      // We were unable to locate an entry with this name.
      return false;
    }
  }
  {
    QSqlQuery username_delete;
    username_delete.prepare("DELETE FROM users WHERE USERNAME = ?");
    username_delete.addBindValue(username);
    username_delete.exec();
    return true;
  }
}

QString kenji::DBManager::getACL(const QString &moderator_name)
{
  if (moderator_name == "")
  {
    return 0;
  }
  QSqlQuery query("SELECT ACL FROM users WHERE USERNAME = ?");
  query.addBindValue(moderator_name);
  query.exec();
  if (!query.first())
  {
    return 0;
  }
  return query.value(0).toString();
}

bool kenji::DBManager::authenticate(const QString &username, const QString &password)
{
  QSqlQuery query_salt("SELECT SALT FROM users WHERE USERNAME = ?");
  query_salt.addBindValue(username);
  query_salt.exec();
  if (!query_salt.first())
  {
    return false;
  }
  QString salt = query_salt.value(0).toString();

  QString salted_password = CryptoHelper::hash_password(QByteArray::fromHex(salt.toUtf8()), password);

  QSqlQuery query_pass("SELECT PASSWORD FROM users WHERE USERNAME = ?");
  query_pass.addBindValue(username);
  query_pass.exec();
  if (!query_pass.first())
  {
    return false;
  }
  QString stored_pass = query_pass.value(0).toString();

  // Update old-style hashes to new ones on the fly
  if (QByteArray::fromHex(salt.toUtf8()).length() < CryptoHelper::pbkdf2_salt_len && salted_password == stored_pass)
  {
    updatePassword(username, password);
  }

  return salted_password == stored_pass;
}

bool kenji::DBManager::updateACL(const QString &f_username, const QString &f_acl)
{
  QSqlQuery l_username_exists;
  l_username_exists.prepare("SELECT ACL FROM users WHERE USERNAME = ?");
  l_username_exists.addBindValue(f_username);
  l_username_exists.exec();

  if (!l_username_exists.first())
  {
    return false;
  }

  QSqlQuery l_update_acl;
  l_update_acl.prepare("UPDATE users SET ACL = ? WHERE USERNAME = ?");
  l_update_acl.addBindValue(f_acl);
  l_update_acl.addBindValue(f_username);
  l_update_acl.exec();
  return true;
}

QStringList kenji::DBManager::getUsers()
{
  QStringList users;

  QSqlQuery query("SELECT USERNAME FROM users ORDER BY ID");
  while (query.next())
  {
    users.append(query.value(0).toString());
  }

  return users;
}

QList<kenji::BanInfo> kenji::DBManager::getBanInfo(const QString &lookup_type, const QString &id)
{
  QList<BanInfo> return_list;
  QSqlQuery query;
  QList<BanInfo> invalid;
  if (lookup_type == "banid")
  {
    query.prepare("SELECT * FROM BANS WHERE ID = ?");
  }
  else if (lookup_type == "hdid")
  {
    query.prepare("SELECT * FROM BANS WHERE HDID = ?");
  }
  else if (lookup_type == "ipid")
  {
    query.prepare("SELECT * FROM BANS WHERE IPID = ?");
  }
  else
  {
    zCritical(log::database) << "Invalid ban lookup type!";
    return invalid;
  }
  query.addBindValue(id);
  query.setForwardOnly(true);
  query.exec();
  while (query.next())
  {
    BanInfo ban;
    ban.id = query.value(0).toInt();
    ban.ipid = query.value(1).toString();
    ban.hdid = query.value(2).toString();
    ban.ip = QHostAddress(query.value(3).toString());
    ban.time = static_cast<unsigned long>(query.value(4).toULongLong());
    ban.reason = query.value(5).toString();
    ban.duration = query.value(6).toLongLong();
    ban.moderator = query.value(7).toString();
    ban.revoked = query.value(8).toBool();
    return_list.append(ban);
  }
  std::reverse(return_list.begin(), return_list.end());
  return return_list;
}

bool kenji::DBManager::updateBan(int ban_id, const QString &field, const QVariant &updated_info)
{
  QSqlQuery query;
  if (field == "reason")
  {
    query.prepare("UPDATE bans SET REASON = ? WHERE ID = ?");
    query.addBindValue(updated_info.toString());
  }
  else if (field == "duration")
  {
    query.prepare("UPDATE bans SET DURATION = ? WHERE ID = ?");
    query.addBindValue(updated_info.toLongLong());
  }
  query.addBindValue(ban_id);
  if (!query.exec())
  {
    zDebug(log::database) << query.lastError();
    return false;
  }
  else
  {
    return true;
  }
}

bool kenji::DBManager::updatePassword(const QString &username, const QString &password)
{
  QByteArray salt = CryptoHelper::randbytes(16);
  QString salted_password = CryptoHelper::hash_password(salt, password);

  QSqlQuery query;
  query.prepare("UPDATE users SET PASSWORD = ?, SALT = ? WHERE USERNAME = ?");
  query.addBindValue(salted_password);
  query.addBindValue(salt.toHex());
  query.addBindValue(username);
  if (!query.exec())
  {
    return false;
  }
  return query.numRowsAffected() > 0;
}

int kenji::DBManager::checkVersion()
{
  QSqlQuery query;
  query.prepare("PRAGMA user_version");
  query.exec();
  if (query.first())
  {
    return query.value(0).toInt();
  }
  else
  {
    return 0;
  }
}

void kenji::DBManager::updateDB(int current_version)
{
  switch (current_version)
  {
  case 0:
    QSqlQuery("ALTER TABLE bans ADD COLUMN MODERATOR TEXT");
    Q_FALLTHROUGH();
  case 1:
    QSqlQuery("PRAGMA user_version = " + QString::number(1));
    Q_FALLTHROUGH();
  case 2:
    QSqlQuery("UPDATE users SET ACL = 'SUPER' WHERE USERNAME = 'root'");
    Q_FALLTHROUGH();
  case 3:
    QSqlQuery("ALTER TABLE bans ADD COLUMN REVOKED INTEGER");
    QSqlQuery("UPDATE bans SET REVOKED = 0 WHERE REVOKED IS NULL");
    QSqlQuery("UPDATE bans SET DURATION = " + QString::number(PermanentBanDuration) + " WHERE DURATION = -2");
    QSqlQuery("PRAGMA user_version = " + QString::number(DB_VERSION));
    break;
  }
}

kenji::DBManager::~DBManager()
{
  db.close();
}
