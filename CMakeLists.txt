cmake_minimum_required(VERSION 3.10)
project(video-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)

if(WIN32)
    set(THIRD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/win32)
    set(FFMPEG_DIR ${THIRD_DIR}/ffmpeg)
    set(GLEW_DIR ${THIRD_DIR}/glew)

    set(INCLUDE_DIR ${THIRD_DIR}/include)

    add_definitions(-DPLATFORM_WINDOWS)

elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    find_package(GLEW REQUIRED)

    add_definitions(-DPLATFORM_LINUX)
endif()

find_package(OpenGL REQUIRED)

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

if(WIN32)
    target_link_libraries(video-client PRIVATE
        ${FFMPEG_DIR}/lib/libavcodec.dll.a
        ${FFMPEG_DIR}/lib/libavformat.dll.a
        ${FFMPEG_DIR}/lib/libavutil.dll.a
        ${GLEW_DIR}/lib/libglew32.dll.a
    )
    target_link_libraries(video-client PRIVATE OpenGL::GL)
    target_link_libraries(video-client PRIVATE ws2_32)

    target_include_directories(video-client PRIVATE ${INCLUDE_DIR})
elseif(UNIX AND NOT APPLE)
    target_link_libraries(video-client PRIVATE
        ${AVCODEC_LIBRARIES}
        ${AVFORMAT_LIBRARIES}
        ${AVUTIL_LIBRARIES}
    )

    target_link_libraries(video-client PRIVATE OpenGL::GL GLEW::GLEW)
endif()
