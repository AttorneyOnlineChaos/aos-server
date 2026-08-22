cmake_minimum_required(VERSION 3.19)
project(kenji VERSION 1.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network WebSockets HttpServer Sql)

if(NOT DEFINED KENJI_OUTPUT_DIRECTORY)
  set(KENJI_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH "")
endif()

set(KENJI_RUNTIME_NAME "kenji-runtime" CACHE STRING "Output filename executable")

qt_standard_project_setup()
qt_add_library(kenji STATIC
  src/commands/area.cpp
  src/commands/authentication.cpp
  src/commands/casing.cpp
  src/commands/command_helper.cpp
  src/commands/messaging.cpp
  src/commands/moderation.cpp
  src/commands/music.cpp
  src/commands/roleplay.cpp
  src/logger/u_logger.cpp
  src/logger/u_logger.h
  src/logger/writer_full.cpp
  src/logger/writer_full.h
  src/logger/writer_modcall.cpp
  src/logger/writer_modcall.h
  src/packet/packet_cc.cpp
  src/packet/packet_ct.cpp
  src/packet/packet_de.cpp
  src/packet/packet_ee.cpp
  src/packet/packet_hi.cpp
  src/packet/packet_hp.cpp
  src/packet/packet_ma.cpp
  src/packet/packet_mc.cpp
  src/packet/packet_ms.cpp
  src/packet/packet_pe.cpp
  src/packet/packet_rt.cpp
  src/packet/packet_zz.cpp
  src/acl_roles_handler.cpp
  src/acl_roles_handler.h
  src/kenji_info.cpp
  src/kenji_info.h
  src/kenji_log.cpp
  src/kenji_log.h
  src/ao_client.cpp
  src/ao_client.h
  src/ao_client_registry.cpp
  src/ao_client_registry.h
  src/area_data.cpp
  src/area_data.h
  src/broadcaster.h
  src/command_extension.cpp
  src/command_extension.h
  src/config_manager.cpp
  src/config_manager.h
  src/connection.cpp
  src/connection.h
  src/connection_pool.cpp
  src/connection_pool.h
  src/crypto_helper.h
  src/data_types.h
  src/db_manager.cpp
  src/db_manager.h
  src/discord.cpp
  src/discord.h
  src/join_floodguard.cpp
  src/join_floodguard.h
  src/medieval_parser.cpp
  src/medieval_parser.h
  src/music_manager.cpp
  src/music_manager.h
  src/packets.cpp
  src/player_state_observer.cpp
  src/player_state_observer.h
  src/server.cpp
  src/server.h
  src/server_publisher.cpp
  src/server_publisher.h
  src/session_registry.cpp
  src/session_registry.h
  src/testimony_recorder.cpp
  src/timer.cpp
  src/timer.h
)

qt_add_executable(kenji-runtime
  src/main.cpp
)

target_include_directories(kenji PUBLIC src)

target_link_libraries(kenji PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
    Qt6::HttpServer
    theory-protocol
)

target_link_libraries(kenji-runtime PRIVATE kenji)

target_compile_definitions(kenji PUBLIC QT_NO_KEYWORDS)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(WIN32)
    set_property(TARGET kenji-runtime PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/data/icon/kenji.rc")
    target_sources(kenji-runtime PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(kenji-runtime PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
  endif()
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

set_target_properties(kenji-runtime PROPERTIES
  OUTPUT_NAME "${KENJI_RUNTIME_NAME}"
  LIBRARY_OUTPUT_DIRECTORY "$<1:${KENJI_OUTPUT_DIRECTORY}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${KENJI_OUTPUT_DIRECTORY}>"
)
