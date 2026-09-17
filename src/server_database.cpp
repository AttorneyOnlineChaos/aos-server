#include "server_database.h"

#include "core/logging.h"
#include "kenji_defs.h"

#include <QDateTime>
#include <QSqlError>

kenji::ServerDatabase::ServerDatabase(const QString &directory)
    : _directory{directory}
    , _connectionName{QStringLiteral("kenji-server-%1").arg(reinterpret_cast<quintptr>(this))}
{}

kenji::ServerDatabase::~ServerDatabase()
{
  if (_open)
  {
    database().close();
    _open = false;
  }

  QSqlDatabase::removeDatabase(_connectionName);
}

std::optional<theory::IOError> kenji::ServerDatabase::open()
{
  const QDir root{_directory};
  if (!root.mkpath(QStringLiteral(".")))
  {
    return theory::IOError::notWritable(QStringLiteral("cannot create directory '%1'").arg(_directory));
  }

  if (const std::optional<theory::IOError> error = openConnection(root))
  {
    QSqlDatabase::removeDatabase(_connectionName);
    return error;
  }

  _open = true;
  return theory::NoIOError;
}

std::optional<theory::IOError> kenji::ServerDatabase::openConnection(const QDir &root)
{
  QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), _connectionName);
  db.setDatabaseName(root.filePath(QStringLiteral("kenji.sqlite")));
  if (!db.open())
  {
    return theory::IOError::notReadable(QStringLiteral("cannot open database: %1").arg(db.lastError().text()));
  }

  QSqlQuery version{db};
  if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())
  {
    const QString reason = version.lastError().text();
    db.close();
    return theory::IOError::notReadable(QStringLiteral("cannot read schema version: %1").arg(reason));
  }

  if (const std::optional<theory::IOError> error = upgrade(db, version.value(0).toInt()))
  {
    db.close();
    return error;
  }

  return theory::NoIOError;
}

std::optional<theory::IOError> kenji::ServerDatabase::upgrade(QSqlDatabase &db, int from)
{
  QStringList statements;
  switch (from)
  {
  default:
    return theory::IOError::malformed(QStringLiteral("schema version %1, expected %2").arg(from).arg(CurrentSchemaVersion));
  case NoSchemaVersion:
    statements += QStringLiteral(R"(
      CREATE TABLE bans (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        subject_id INTEGER NOT NULL,
        issuer_id INTEGER NOT NULL,
        issued_on INTEGER NOT NULL,
        duration INTEGER NOT NULL,
        reason TEXT NOT NULL,
        revoker_id INTEGER
      )
    )");
    statements += QStringLiteral("CREATE INDEX bans_by_subject ON bans(subject_id)");
    statements += QStringLiteral(R"(
      CREATE TABLE roles (
        user_id INTEGER PRIMARY KEY,
        role TEXT NOT NULL
      )
    )");
    statements += QStringLiteral(R"(
      CREATE TABLE names (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        seen_on INTEGER NOT NULL
      )
    )");
    statements += QStringLiteral("CREATE INDEX names_by_user ON names(user_id, id)");
    [[fallthrough]];
  case SchemaVersion1:
    break;
  }

  statements += QStringLiteral("PRAGMA user_version = %1").arg(CurrentSchemaVersion);

  if (!db.transaction())
  {
    return theory::IOError::notWritable(QStringLiteral("cannot begin transaction: %1").arg(db.lastError().text()));
  }

  for (const QString &statement : statements)
  {
    QSqlQuery query{db};
    if (!query.exec(statement))
    {
      const QString reason = query.lastError().text();
      db.rollback();
      return theory::IOError::notWritable(QStringLiteral("cannot upgrade schema: %1").arg(reason));
    }
  }

  if (!db.commit())
  {
    const QString reason = db.lastError().text();
    db.rollback();
    return theory::IOError::notWritable(QStringLiteral("cannot commit schema: %1").arg(reason));
  }

  return theory::NoIOError;
}

std::optional<theory::IOError> kenji::ServerDatabase::execute(QSqlQuery &query)
{
  if (!query.exec())
  {
    return theory::IOError::notWritable(query.lastError().text());
  }

  return theory::NoIOError;
}

theory::BanRecord kenji::ServerDatabase::readBan(const QSqlQuery &query)
{
  theory::BanRecord ban;
  ban.id = query.value(0).toInt();
  ban.subjectId = query.value(1).toInt();
  ban.issuerId = query.value(2).toInt();
  ban.issuedOn = query.value(3).toLongLong();
  ban.duration = query.value(4).toLongLong();
  ban.reason = query.value(5).toString();
  if (!query.value(6).isNull())
  {
    ban.revokerId = query.value(6).toInt();
  }

  return ban;
}

std::optional<QList<theory::BanRecord>> kenji::ServerDatabase::readBans(QSqlQuery &query)
{
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("ban lookup failed: %1").arg(query.lastError().text());
    return std::nullopt;
  }

  QList<theory::BanRecord> bans;
  while (query.next())
  {
    bans.append(readBan(query));
  }

  return bans;
}

std::optional<QList<theory::BanRecord>> kenji::ServerDatabase::activeBans(theory::UserId subjectId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("SELECT id, subject_id, issuer_id, issued_on, duration, reason, revoker_id FROM bans WHERE subject_id = ? AND revoker_id IS NULL AND issued_on + duration > ? ORDER BY issued_on DESC, id DESC"));
  query.addBindValue(subjectId);
  query.addBindValue(QDateTime::currentSecsSinceEpoch());
  return readBans(query);
}

std::optional<theory::BanRecord> kenji::ServerDatabase::ban(theory::BanId banId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("SELECT id, subject_id, issuer_id, issued_on, duration, reason, revoker_id FROM bans WHERE id = ?"));
  query.addBindValue(banId);
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("ban lookup failed: %1").arg(query.lastError().text());
    return std::nullopt;
  }

  if (!query.next())
  {
    return std::nullopt;
  }

  return readBan(query);
}

std::optional<QList<theory::BanRecord>> kenji::ServerDatabase::recentBans(int count)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("SELECT id, subject_id, issuer_id, issued_on, duration, reason, revoker_id FROM (SELECT * FROM bans ORDER BY id DESC LIMIT ?) ORDER BY id ASC"));
  query.addBindValue(count);
  return readBans(query);
}

std::optional<theory::BanId> kenji::ServerDatabase::addBan(const theory::BanRecord &ban)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("INSERT INTO bans (subject_id, issuer_id, issued_on, duration, reason) VALUES (?, ?, ?, ?, ?)"));
  query.addBindValue(ban.subjectId);
  query.addBindValue(ban.issuerId);
  query.addBindValue(ban.issuedOn);
  query.addBindValue(ban.duration);
  query.addBindValue(ban.reason);
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("ban insert failed: %1").arg(query.lastError().text());
    return std::nullopt;
  }

  return query.lastInsertId().toInt();
}

std::optional<theory::IOError> kenji::ServerDatabase::revokeBan(theory::BanId banId, theory::UserId revokerId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("UPDATE bans SET revoker_id = ? WHERE id = ?"));
  query.addBindValue(revokerId);
  query.addBindValue(banId);
  return execute(query);
}

std::optional<theory::IOError> kenji::ServerDatabase::setBanReason(theory::BanId banId, const QString &reason)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("UPDATE bans SET reason = ? WHERE id = ?"));
  query.addBindValue(reason);
  query.addBindValue(banId);
  return execute(query);
}

std::optional<theory::IOError> kenji::ServerDatabase::setBanDuration(theory::BanId banId, theory::BanDuration duration)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("UPDATE bans SET duration = ? WHERE id = ?"));
  query.addBindValue(duration);
  query.addBindValue(banId);
  return execute(query);
}

std::optional<QString> kenji::ServerDatabase::role(theory::UserId userId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("SELECT role FROM roles WHERE user_id = ?"));
  query.addBindValue(userId);
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("role lookup failed: %1").arg(query.lastError().text());
    return std::nullopt;
  }

  if (!query.next())
  {
    return std::nullopt;
  }

  return query.value(0).toString();
}

std::optional<theory::IOError> kenji::ServerDatabase::setRole(theory::UserId userId, const QString &role)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("INSERT INTO roles (user_id, role) VALUES (?, ?) ON CONFLICT(user_id) DO UPDATE SET role = excluded.role"));
  query.addBindValue(userId);
  query.addBindValue(role);
  return execute(query);
}

std::optional<theory::IOError> kenji::ServerDatabase::clearRole(theory::UserId userId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("DELETE FROM roles WHERE user_id = ?"));
  query.addBindValue(userId);
  return execute(query);
}

std::optional<QString> kenji::ServerDatabase::lastName(theory::UserId userId)
{
  QSqlQuery query{database()};
  query.prepare(QStringLiteral("SELECT name FROM names WHERE user_id = ? ORDER BY id DESC LIMIT 1"));
  query.addBindValue(userId);
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("name lookup failed: %1").arg(query.lastError().text());
    return std::nullopt;
  }

  if (!query.next())
  {
    return std::nullopt;
  }

  return query.value(0).toString();
}

void kenji::ServerDatabase::recordName(theory::UserId userId, const QString &name)
{
  if (lastName(userId) == name)
  {
    return;
  }

  QSqlQuery query{database()};
  query.prepare(QStringLiteral("INSERT INTO names (user_id, name, seen_on) VALUES (?, ?, ?)"));
  query.addBindValue(userId);
  query.addBindValue(name);
  query.addBindValue(QDateTime::currentSecsSinceEpoch());
  if (!query.exec())
  {
    zWarning(log::database) << QStringLiteral("name insert failed: %1").arg(query.lastError().text());
  }
}

QSqlDatabase kenji::ServerDatabase::database() const
{
  return QSqlDatabase::database(_connectionName);
}
