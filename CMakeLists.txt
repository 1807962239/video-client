cmake_minimum_required(VERSION 3.10)
project(video-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

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

add_executable(video-client ${CPP_SOURCES} ${CPP_HEADERS}
    res.qrc
)

target_link_libraries(video-client PRIVATE Qt6::Core Qt6::Widgets Qt6::OpenGLWidgets)

target_link_libraries(video-client PRIVATE
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
)

target_link_libraries(video-client PRIVATE OpenGL::GL GLEW::GLEW)
