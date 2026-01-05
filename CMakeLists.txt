cmake_minimum_required(VERSION 3.10)
project(video-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 不支持Qt6则使用Qt5
find_package(Qt6 QUIET COMPONENTS Widgets Core OpenGLWidgets)
if(Qt6_FOUND)
    set(QT_VERSION 6)
    set(QT_LIBRARIES Qt6::Core Qt6::Widgets Qt6::OpenGLWidgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Core OpenGL)
    set(QT_VERSION 5)
    set(QT_LIBRARIES Qt5::Core Qt5::Widgets Qt5::OpenGL)
endif()

find_package(OpenGL REQUIRED)

set(THIRD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/win32)

if(WIN32)
    set(FFMPEG_LIBS ${THIRD_DIR}/ffmpeg/lib/libavcodec.dll.a
                    ${THIRD_DIR}/ffmpeg/lib/libavformat.dll.a
                    ${THIRD_DIR}/ffmpeg/lib/libavutil.dll.a)
    set(GLEW_LIBS ${THIRD_DIR}/glew/lib/libglew32.dll.a)
    set(SYSTEM_LIBS ws2_32)

elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
    )
    find_package(GLEW REQUIRED)
    find_package(Threads REQUIRED)

    set(FFMPEG_LIBS PkgConfig::FFMPEG)
    set(GLEW_LIBS GLEW::GLEW)

endif()

set(CPP_SOURCES
    main.cpp
    videoclient.cpp
    mainwindow.cpp
    h264decoder.cpp
    openglwidget.cpp
)

set(CPP_HEADERS
    type.h
    videoclient.h
    mainwindow.h
    h264decoder.h
    openglwidget.h
)

add_executable(video-client ${CPP_SOURCES} ${CPP_HEADERS})

if(QT_VERSION EQUAL 6)
    qt_add_resources(video_client_resources res.qrc)
    target_sources(video-client PRIVATE ${video_client_resources})
else()
    qt5_add_resources(video_client_resources res.qrc)
    target_sources(video-client PRIVATE ${video_client_resources})
endif()

target_link_libraries(video-client PRIVATE
    ${QT_LIBRARIES}
    OpenGL::GL
    ${FFMPEG_LIBS}
    ${GLEW_LIBS}
)

if (WIN32)
    target_include_directories(video-client PRIVATE ${THIRD_DIR}/include)
    target_link_libraries(video-client PRIVATE ${SYSTEM_LIBS})
elseif (UNIX AND NOT APPLE)
    target_link_libraries(video-client PRIVATE Threads::Threads)
endif()
