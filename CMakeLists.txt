cmake_minimum_required(VERSION 4.2)

project(EremeevDiploma)

add_executable(Client)
target_sources(Client
  PRIVATE
    main.c codec.c net.c
)

find_package(SDL3 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

target_link_libraries(Client PRIVATE SDL3::SDL3 PkgConfig::LIBAV)

add_executable(Server)
target_sources(Server
  PRIVATE
    server.c net.c s_conn_map.c
)
