include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG        master  # or latest stable
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Use standalone Asio (header-only)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-26-0
)
FetchContent_MakeAvailable(asio)


add_library(dependencies INTERFACE)


target_include_directories(dependencies
INTERFACE
        ${glfw_SOURCE_DIR}/include
)

target_link_libraries(dependencies
INTERFACE
        glfw
        asio
)
