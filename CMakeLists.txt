cmake_minimum_required(VERSION 3.20)
project(CoinCollectorMP VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------
# SDL2 (pre-built)
# ---------------------------
set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2-install/lib/cmake/SDL2)
find_package(SDL2 REQUIRED)

# ---------------------------
# SDL2_ttf
# ---------------------------
# set(SDL2_ttf_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2_ttf-install/lib/cmake/SDL2_ttf)
# find_package(SDL2_ttf REQUIRED)

# ---------------------------
# Standalone Asio (header-only)
# ---------------------------
add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include/asio.hpp")
    message(WARNING "Asio header not found at: ${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include/asio.hpp")
endif()

# ---------------------------
# Common interface library
# ---------------------------
add_library(common INTERFACE)
target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(common INTERFACE asio)

# ---------------------------
# Server executable
# ---------------------------
add_executable(server
    server/main.cpp
    server/server.cpp
    server/session.cpp
)
target_link_libraries(server PRIVATE common)
target_compile_definitions(server PRIVATE ASIO_NO_DEPRECATED)

# ---------------------------
# Client executable
# ---------------------------
add_executable(client
    client/main.cpp
    client/client.cpp
    client/renderer.cpp
) 
target_link_libraries(client PRIVATE common SDL2::SDL2) # 
target_compile_definitions(client PRIVATE ASIO_NO_DEPRECATED)

# ---------------------------
# Windows specifics
# ---------------------------
if (WIN32)
    if (TARGET SDL2::SDL2main)
        target_link_libraries(client PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(server PRIVATE ws2_32 wsock32)
    target_link_libraries(client PRIVATE ws2_32 wsock32)
endif()

message(STATUS "Configured project: ${PROJECT_NAME} (C++${CMAKE_CXX_STANDARD})")