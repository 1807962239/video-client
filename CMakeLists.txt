cmake_minimum_required(VERSION 3.10)
project(video-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPP_SOURCE
    main.cpp
    videoclient.cpp
)

add_executable(video-client ${CPP_SOURCE})