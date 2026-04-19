cmake_minimum_required(VERSION 4.2)

project(EremeevDiploma LANGUAGES C)


add_executable(Client)
target_sources(Client
  PRIVATE
    main.c
    codec.c
    device.c
    shared/net.c
    widgets/picture_widget.c
    widgets/nuklear.c
    user_list.c
)

# Nuklear (single-header, no linking needed)
target_sources(Client
  PRIVATE
    nuklear.h
    nuklear_sdl3_renderer.h
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

# Копирование шрифта в директорию сборки
add_custom_command(TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/NotoSans-Regular.ttf
    $<TARGET_FILE_DIR:Client>/NotoSans-Regular.ttf
)

