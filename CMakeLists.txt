cmake_minimum_required(VERSION 3.10)
project(video-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(CPP_SOURCES
    main.cpp
    videoclient.cpp
    mainwindow.cpp
)

set(CPP_HEADERS
    type.h
    videoclient.h
    mainwindow.h
)

add_executable(video-client ${CPP_SOURCES} ${CPP_HEADERS})

target_link_libraries(video-client PRIVATE Qt6::Core Qt6::Widgets)
target_link_libraries(video-client PRIVATE Qt6::Widgets)
