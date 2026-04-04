cmake_minimum_required(VERSION 3.24)
project(conduit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# options
option(CONDUIT_BUILD_TESTS "Build test suite" ON)
option(CONDUIT_USE_VCPKG "Use vcpkg for dependencies" ON)

# --- dependencies via vcpkg / find_package ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)

# libwebsockets is a pain to find_package on windows, we'll deal with it later
# find_package(Libwebsockets REQUIRED)

# --- header-only / small deps via FetchContent ---
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# --- Dear ImGui as a static library (SDL2 + OpenGL3 backends) ---
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 OpenGL::GL)

# --- main executable ---
file(GLOB_RECURSE CONDUIT_SOURCES src/*.cpp)
add_executable(conduit WIN32 ${CONDUIT_SOURCES})

target_include_directories(conduit PRIVATE
    src/
    ${stb_SOURCE_DIR}
)

target_link_libraries(conduit PRIVATE
    imgui_lib
    SDL2::SDL2
    SDL2::SDL2main
    OpenGL::GL
    CURL::libcurl
    SQLite3::SQLite3
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

# copy assets to build dir so the exe can find them
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
file(COPY config/ DESTINATION ${CMAKE_BINARY_DIR}/config/)

# tests (someday)
if(CONDUIT_BUILD_TESTS)
    enable_testing()
    # add_subdirectory(tests)
endif()
