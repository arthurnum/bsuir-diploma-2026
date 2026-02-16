cmake_minimum_required(VERSION 4.2)

project(EremeevDiploma)

add_executable(Server)
target_sources(Server
  PRIVATE
    server.c
    s_conn_map.c
    ../shared/net.c
)
