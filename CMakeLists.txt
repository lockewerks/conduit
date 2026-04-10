cmake_minimum_required(VERSION 3.24)
project(conduit VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# windows min/max macros are the bane of existence
if(WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

# options
option(CONDUIT_BUILD_TESTS "Build test suite" ON)
option(CONDUIT_USE_VCPKG "Use vcpkg for dependencies" ON)

# --- dependencies ---
# use find_package first (works with vcpkg), fall back to pkg-config for
# system packages on Linux that don't ship cmake config files.
find_package(PkgConfig QUIET)

find_package(OpenGL REQUIRED)
find_package(GIF REQUIRED)
find_package(nlohmann_json REQUIRED)

# SDL2
find_package(SDL2 QUIET)
if(NOT TARGET SDL2::SDL2)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_PC REQUIRED IMPORTED_TARGET sdl2)
        add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2_PC)
        add_library(sdl2main_stub INTERFACE)
        add_library(SDL2::SDL2main ALIAS sdl2main_stub)
    else()
        message(FATAL_ERROR "SDL2 not found")
    endif()
endif()

# CURL
find_package(CURL QUIET)
if(NOT TARGET CURL::libcurl)
    if(PkgConfig_FOUND)
        pkg_check_modules(CURL_PC REQUIRED IMPORTED_TARGET libcurl)
        add_library(CURL::libcurl ALIAS PkgConfig::CURL_PC)
    else()
        message(FATAL_ERROR "CURL not found")
    endif()
endif()

# SQLite3 — vcpkg uses unofficial-sqlite3, system uses SQLite3
find_package(SQLite3 QUIET)
if(NOT TARGET SQLite3::SQLite3)
    find_package(unofficial-sqlite3 CONFIG QUIET)
    if(TARGET unofficial::sqlite3::sqlite3)
        add_library(SQLite3::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
    elseif(PkgConfig_FOUND)
        pkg_check_modules(SQLITE3_PC REQUIRED IMPORTED_TARGET sqlite3)
        add_library(SQLite3::SQLite3 ALIAS PkgConfig::SQLITE3_PC)
    else()
        message(FATAL_ERROR "SQLite3 not found")
    endif()
endif()

# libwebsockets
find_package(libwebsockets CONFIG QUIET)
if(NOT TARGET websockets_shared AND NOT TARGET websockets)
    if(PkgConfig_FOUND)
        pkg_check_modules(LWS REQUIRED IMPORTED_TARGET libwebsockets)
    else()
        message(FATAL_ERROR "libwebsockets not found")
    endif()
endif()

# LevelDB (for reading Chromium localStorage)
find_package(leveldb CONFIG QUIET)

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
# 32-bit wchar so ImGui can address emoji codepoints above U+FFFF
target_compile_definitions(imgui_lib PUBLIC IMGUI_USE_WCHAR32)
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
    GIF::GIF
)

# link leveldb if found
if(TARGET leveldb::leveldb)
    target_link_libraries(conduit PRIVATE leveldb::leveldb)
    target_compile_definitions(conduit PRIVATE CONDUIT_HAS_LEVELDB=1)
endif()

# link libwebsockets — target name depends on how it was found
if(TARGET websockets_shared)
    target_link_libraries(conduit PRIVATE websockets_shared)
elseif(TARGET websockets)
    target_link_libraries(conduit PRIVATE websockets)
elseif(TARGET PkgConfig::LWS)
    target_link_libraries(conduit PRIVATE PkgConfig::LWS)
else()
    message(FATAL_ERROR "libwebsockets not found")
endif()

# macOS framework includes for OpenGL
if(APPLE)
    target_compile_definitions(conduit PRIVATE GL_SILENCE_DEPRECATION)
endif()

# MSVC multi-config generators put output in Debug/ or Release/ subdirs
foreach(CONFIG_TYPE Debug Release RelWithDebInfo MinSizeRel)
    file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/${CONFIG_TYPE}/assets/)
    file(COPY config/ DESTINATION ${CMAKE_BINARY_DIR}/${CONFIG_TYPE}/config/)
endforeach()
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
file(COPY config/ DESTINATION ${CMAKE_BINARY_DIR}/config/)

if(CONDUIT_BUILD_TESTS)
    enable_testing()
endif()
