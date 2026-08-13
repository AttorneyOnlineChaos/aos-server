#include "serverpublisher.h"
#include "config_manager.h"
#include "core/logging.h"
#include "kenji_log.h"
#include "qnamespace.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

const int HTTP_OK = 200;
const int WS_REVERSE_PROXY = 80;
const int TIMEOUT = 1000 * 60 * 4;

kenji::ServerPublisher::ServerPublisher(int port, int *player_count, QObject *parent)
    : QObject(parent)
    , m_manager{new QNetworkAccessManager(this)}
    , timeout_timer(new QTimer(this))
    , m_players(player_count)
    , m_port{port}
{
  connect(m_manager, &QNetworkAccessManager::finished, this, &ServerPublisher::finished);
  connect(timeout_timer, &QTimer::timeout, this, &ServerPublisher::publishServer);

  timeout_timer->setTimerType(Qt::PreciseTimer);
  timeout_timer->setInterval(TIMEOUT);
  timeout_timer->start();
  publishServer();
}

void kenji::ServerPublisher::publishServer()
{
  if (!ConfigManager::publishServerEnabled())
  {
    return;
  }

  QUrl serverlist(ConfigManager::serverlistURL());
  if (serverlist.isValid())
  {
    QNetworkRequest request(serverlist);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject serverinfo;
    if (!ConfigManager::serverDomainName().trimmed().isEmpty())
    {
      serverinfo["ip"] = ConfigManager::serverDomainName();
    }
    if (ConfigManager::securePort() != -1)
    {
      serverinfo["wss_port"] = ConfigManager::securePort();
    }
    serverinfo["port"] = 27106;
    serverinfo["ws_port"] = ConfigManager::advertiseWSProxy() ? WS_REVERSE_PROXY : m_port;
    serverinfo["players"] = *m_players;
    serverinfo["name"] = ConfigManager::serverName();
    serverinfo["description"] = ConfigManager::serverDescription();

    m_manager->post(request, QJsonDocument(serverinfo).toJson());
  }
  else
  {
    zWarning(log::master) << "Failed to advertise server. Serverlist URL is not valid. URL:" << serverlist.toString();
  }
}

void kenji::ServerPublisher::finished(QNetworkReply *f_reply)
{
  QNetworkReply *reply(f_reply);
  reply->deleteLater();
  QString remote_url = reply->url().toString();

  if (reply->error() != QNetworkReply::NoError)
  {
    zWarning(log::master) << "Unable to connect to serverlist due to the following error:" << reply->errorString();
    zWarning(log::master) << "Remote URL:" << remote_url;
    return;
  }

  QByteArray data = reply->isReadable() ? reply->readAll() : QByteArray();
  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (status != HTTP_OK)
  {
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
      zWarning(log::master) << "Received malformed response from masterserver. Error:" << error.errorString();
      zWarning(log::master) << "HTTP status code:" << status;
      zWarning(log::master) << "Parse error offset:" << error.offset;
      zWarning(log::master) << "Response body size:" << data.size() << "bytes";
      zWarning(log::master) << "Raw response body:" << QString::fromUtf8(data);
      return;
    }

    QJsonObject body = document.object();
    if (body.contains("errors"))
    {
      zWarning(log::master) << "Failed to advertise to the serverlist due to the following errors:";
      const QJsonArray errors = body["errors"].toArray();
      for (const auto &ref : errors)
      {
        QJsonObject error = ref.toObject();
        zWarning(log::master) << "Error:" << error["type"].toString() << ". Message:" << error["message"].toString();
      }
      return;
    }
  }
  zInfo(log::master) << "Sucessfully advertised server to serverlist.";
}
