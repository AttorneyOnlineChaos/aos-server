#pragma once

#include "badge/badge_defs.h"
#include "core/io_error.h"
#include "game/ban_record.h"
#include "game/game_defs.h"

#include <QDir>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include <optional>

namespace kenji
{
class ServerDatabase
{
public:
  explicit ServerDatabase(const QString &directory);
  ServerDatabase(const ServerDatabase &) = delete;
  ServerDatabase &operator=(const ServerDatabase &) = delete;
  ~ServerDatabase();

  std::optional<theory::IOError> open();

  std::optional<QList<theory::BanRecord>> activeBans(theory::UserId subjectId);
  std::optional<theory::BanRecord> ban(theory::BanId banId);
  std::optional<QList<theory::BanRecord>> recentBans(int count);
  std::optional<theory::BanId> addBan(const theory::BanRecord &ban);
  std::optional<theory::IOError> revokeBan(theory::BanId banId, theory::UserId revokerId);
  std::optional<theory::IOError> setBanReason(theory::BanId banId, const QString &reason);
  std::optional<theory::IOError> setBanDuration(theory::BanId banId, theory::BanDuration duration);

  std::optional<QString> role(theory::UserId userId);
  std::optional<theory::IOError> setRole(theory::UserId userId, const QString &role);
  std::optional<theory::IOError> clearRole(theory::UserId userId);

  std::optional<QString> lastName(theory::UserId userId);
  void recordName(theory::UserId userId, const QString &name);

private:
  enum SchemaVersion
  {
    NoSchemaVersion,
    SchemaVersion1,
    SchemaVersionCount,
    CurrentSchemaVersion = SchemaVersionCount - 1,
  };

  QString _directory;
  QString _connectionName;
  bool _open = false;

  static std::optional<theory::IOError> upgrade(QSqlDatabase &db, int from);
  static std::optional<theory::IOError> execute(QSqlQuery &query);
  static theory::BanRecord readBan(const QSqlQuery &query);

  std::optional<theory::IOError> openConnection(const QDir &root);
  std::optional<QList<theory::BanRecord>> readBans(QSqlQuery &query);
  QSqlDatabase database() const;
};
} // namespace kenji
