cmake_minimum_required(VERSION 3.20)
project(CoinCollectorMP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------
# SDL2 (prebuilt)
# ---------------------------
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2-install/lib/cmake/SDL2")
message(STATUS "Looking for SDL2Config in: ${SDL2_DIR}")
find_package(SDL2 REQUIRED)

if (TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
else()
    message(FATAL_ERROR "SDL2 CMake target not found (expected SDL2::SDL2 or SDL2::SDL2-static).")
endif()

# ---------------------------
# SDL2_ttf (resilient find + fallback)
# ---------------------------
set(SDL2_ttf_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2_ttf-install/lib/cmake/SDL2_ttf")
message(STATUS "Looking for SDL2_ttfConfig in: ${SDL2_ttf_DIR}")

# Try to find the package by config first (quiet so we can fallback)
find_package(SDL2_ttf CONFIG QUIET)

if (TARGET SDL2_ttf::SDL2_ttf)
    message(STATUS "Found SDL2_ttf target via find_package.")
else()
    message(STATUS "SDL2_ttf::SDL2_ttf NOT found by find_package, trying fallback detection...")

    # Look for installed layout (headers + lib)
    set(POSSIBLE_TTF_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2_ttf-install")
    set(POSSIBLE_INC "${POSSIBLE_TTF_INSTALL_DIR}/include")
    set(POSSIBLE_LIB_DIR "${POSSIBLE_TTF_INSTALL_DIR}/lib")

    message(STATUS "Checking fallback paths: include=${POSSIBLE_INC}, lib=${POSSIBLE_LIB_DIR}")

    # try to find header and static library
    find_path(SDL2_TTF_FALLBACK_INCLUDE_DIR SDL2/SDL_ttf.h PATHS "${POSSIBLE_INC}" NO_DEFAULT_PATH)
    find_library(SDL2_TTF_FALLBACK_LIB NAMES SDL2_ttf SDL2_ttf_static SDL2_ttf.a PATHS "${POSSIBLE_LIB_DIR}" NO_DEFAULT_PATH)

    if (SDL2_TTF_FALLBACK_INCLUDE_DIR AND SDL2_TTF_FALLBACK_LIB)
        message(STATUS "SDL2_ttf fallback found: include=${SDL2_TTF_FALLBACK_INCLUDE_DIR}, lib=${SDL2_TTF_FALLBACK_LIB}")

        # Create an imported target so the rest of the CMake can link normally.
        add_library(SDL2_ttf::SDL2_ttf STATIC IMPORTED GLOBAL)
        set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
            IMPORTED_LOCATION "${SDL2_TTF_FALLBACK_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_FALLBACK_INCLUDE_DIR}"
        )
    else()
        # Final attempt: maybe the installed package is at slightly different path; try to print diagnostic and error.
        message(STATUS "SDL2_ttf fallback detection failed.")
        if (EXISTS "${SDL2_ttf_DIR}")
            file(GLOB cmake_files "${SDL2_ttf_DIR}/*")
            message(STATUS "Contents of SDL2_ttf_DIR (${SDL2_ttf_DIR}): ${cmake_files}")
        endif()

        message(FATAL_ERROR "SDL2_ttf not found. Either install SDL2_ttf (and its CMake config) or place headers/libs under third-party/SDL2_ttf-install.\n\
Suggested fixes:\n\
  * Ensure 'third-party/SDL2_ttf-install/lib/cmake/SDL2_ttf/SDL2_ttfConfig.cmake' exists OR\n\
  * Put SDL2_ttf headers into third-party/SDL2_ttf-install/include and libs into third-party/SDL2_ttf-install/lib\n\
  * OR let me create the targets from exact paths if you paste the paths to SDL2_ttf include and lib.")
    endif()
endif()

# Confirm we have a usable target now
if (NOT TARGET SDL2_ttf::SDL2_ttf)
    message(FATAL_ERROR "SDL2_ttf::SDL2_ttf target is still missing after fallback. CMake will stop.")
endif()

# ---------------------------
# Standalone Asio (header-only)
# ---------------------------
add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include")

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include/asio.hpp")
    message(WARNING "Asio header not found at: ${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio-1.36.0/include/asio.hpp")
endif()

# ---------------------------
# Common interface library
# ---------------------------
add_library(common INTERFACE)
target_include_directories(common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/common")
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

find_library(FREETYPE_LIB 
    NAMES freetype libfreetype freetypelib
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2_ttf-install/lib"
    NO_DEFAULT_PATH
)

if(NOT FREETYPE_LIB)
    message(FATAL_ERROR "FreeType library not found in SDL2_ttf-install/lib")
endif()

message(STATUS "Found FreeType library: ${FREETYPE_LIB}")

# Link SDL2 + SDL2_ttf for the client. Use the chosen SDL2 target (static if available).
target_link_libraries(client PRIVATE
    common
    ${SDL2_TARGET}
    SDL2_ttf::SDL2_ttf
    ${FREETYPE_LIB}
)
target_compile_definitions(client PRIVATE ASIO_NO_DEPRECATED USE_SDL_TTF)

# ---------------------------
# Windows specifics
# ---------------------------
if (WIN32)
    # Link SDL2::SDL2main only to the client (programs that use SDL)
    if (TARGET SDL2::SDL2main)
        target_link_libraries(client PRIVATE SDL2::SDL2main)
    endif()

    # Win sock libs for networking (server uses asio)
    target_link_libraries(server PRIVATE ws2_32 wsock32)
    target_link_libraries(client PRIVATE ws2_32 wsock32)
endif()

# ---------------------------
# Helpful configuration messages
# ---------------------------
message(STATUS "Configured project: ${PROJECT_NAME} (C++${CMAKE_CXX_STANDARD})")
message(STATUS "Using SDL2 target: ${SDL2_TARGET}")
message(STATUS "Using SDL2_ttf target: SDL2_ttf::SDL2_ttf")
