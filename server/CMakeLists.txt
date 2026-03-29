cmake_minimum_required(VERSION 3.16)

project(EremeevDiploma LANGUAGES C)

add_executable(Server)
target_sources(Server
  PRIVATE
    server.c
    s_conn_map.c
    ../shared/net.c
)

if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32)
endif()
