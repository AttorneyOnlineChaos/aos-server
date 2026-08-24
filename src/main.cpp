#include "config_manager.h"
#include "core/logging.h"
#include "kenji_log.h"
#include "network/packet_factory.h"
#include "protocol/protocol_utils.h"
#include "server.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstdlib>

kenji::Server *server;

void cleanup()
{
  server->deleteLater();
}

int main(int argc, char *argv[])
{
  kenji::ConfigManager config;

  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("kenji");
  QCoreApplication::setApplicationVersion("jackfruit (1.9)");
  std::atexit(cleanup);

  theory::PacketFactory packet_factory;
  theory::registerPackets(packet_factory);

  // Verify server configuration is sound.
  if (!kenji::ConfigManager::verifyServerConfig())
  {
    zCritical(kenji::log::main) << "config.ini is invalid!";
    zCritical(kenji::log::main) << "Exiting server due to configuration issue.";
    exit(EXIT_FAILURE);
  }
  else
  {
    server = new kenji::Server(kenji::ConfigManager::serverPort(), packet_factory, &app);
    if (!server->start())
    {
      zCritical(kenji::log::main) << "server failed to start";
      return EXIT_FAILURE;
    }
  }

  return app.exec();
}
