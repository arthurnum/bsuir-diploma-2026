cmake_minimum_required(VERSION 3.16)

project(EremeevDiploma LANGUAGES C)


add_executable(Client)
target_sources(Client
  PRIVATE
    main.c
    codec.c
    device.c
    shared/net.c
)

# SDL3
find_package(SDL3 REQUIRED)
target_include_directories(Client PRIVATE ${SDL3_INCLUDE_DIRS})
target_link_libraries(Client PRIVATE ${SDL3_LIBRARIES})

# FFmpeg
if(WIN32)
    # Windows: указать пути к FFmpeg вручную или через vcpkg
    # Вариант 1: через vcpkg (если установлен)
    # find_package(FFmpeg REQUIRED)
    # target_link_libraries(Client PRIVATE FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil FFmpeg::swresample FFmpeg::swscale FFmpeg::avdevice FFmpeg::avfilter)

    # Вариант 2: ручное указание путей
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}" CACHE PATH "Path to FFmpeg")
    if(NOT FFMPEG_ROOT)
        message(FATAL_ERROR "FFMPEG_ROOT environment variable not set. Set it to the root of your FFmpeg installation.")
    endif()

    set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIB_DIRS "${FFMPEG_ROOT}/lib")

    target_include_directories(Client PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(Client PRIVATE
        "${FFMPEG_LIB_DIRS}/avcodec.lib"
        "${FFMPEG_LIB_DIRS}/avformat.lib"
        "${FFMPEG_LIB_DIRS}/avutil.lib"
        "${FFMPEG_LIB_DIRS}/swresample.lib"
        "${FFMPEG_LIB_DIRS}/swscale.lib"
        "${FFMPEG_LIB_DIRS}/avdevice.lib"
        "${FFMPEG_LIB_DIRS}/avfilter.lib"
    )

    # Для Windows также может понадобиться ws2_32.lib для сокетов
    target_link_libraries(Client PRIVATE ws2_32)
else()
    # Linux/macOS: используем PkgConfig
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
    target_include_directories(Client PRIVATE ${LIBAV_INCLUDE_DIRS})
    target_link_libraries(Client PRIVATE ${LIBAV_LIBRARIES})
endif()

