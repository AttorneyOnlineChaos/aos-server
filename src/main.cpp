#include "config_manager.h"
#include "core/logging.h"
#include "kenji_defs.h"
#include "network/packet_factory.h"
#include "protocol/protocol_utils.h"
#include "server.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstdlib>

int main(int argc, char *argv[])
{
  int code = 0;
  {
    kenji::ConfigManager config;

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("kenji");
    QCoreApplication::setApplicationVersion("jackfruit (1.9)");

    theory::PacketFactory packet_factory;
    theory::registerPackets(packet_factory);

    // Verify server configuration is sound.
    if (!kenji::ConfigManager::verifyServerConfig())
    {
      zCritical(kenji::log::main) << "config.ini is invalid!";
      zCritical(kenji::log::main) << "Exiting server due to configuration issue.";
      return EXIT_FAILURE;
    }

    kenji::Server server{kenji::ConfigManager::serverPort(), packet_factory};
    if (!server.start())
    {
      zCritical(kenji::log::main) << "server failed to start";
      return EXIT_FAILURE;
    }

    code = app.exec();
  }

  return code;
}
