cmake_minimum_required(VERSION 3.16)
project(Agar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8 /W3)
else()
    add_compile_options(-Wall -Wextra)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

option(AGAR_BUILD_SERVER "Собрать сервер (libuv)" ON)
option(AGAR_BUILD_CLIENT "Собрать клиент (SFML)" ON)
option(AGAR_BUILD_TESTS  "Собрать тесты логики" ON)

# ---------------------------------------------------------------- общий код
add_library(agar_common STATIC
    common/game_world.cpp
    common/records.cpp)
target_include_directories(agar_common PUBLIC common)

# ---------------------------------------------------------------- сервер (libuv)
if(AGAR_BUILD_SERVER)
    set(AGAR_LIBUV_TARGET "")

    find_package(libuv CONFIG QUIET)                 # vcpkg, conan, установленный через cmake
    if(TARGET libuv::uv)
        set(AGAR_LIBUV_TARGET libuv::uv)
    elseif(TARGET libuv::uv_a)
        set(AGAR_LIBUV_TARGET libuv::uv_a)
    endif()

    if(NOT AGAR_LIBUV_TARGET)
        find_package(PkgConfig QUIET)                # apt install libuv1-dev
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBUV QUIET IMPORTED_TARGET libuv)
            if(LIBUV_FOUND)
                set(AGAR_LIBUV_TARGET PkgConfig::LIBUV)
            endif()
        endif()
    endif()

    if(NOT AGAR_LIBUV_TARGET)
        find_path(LIBUV_INCLUDE_DIR uv.h)            # можно задать -DLIBUV_INCLUDE_DIR=... -DLIBUV_LIBRARY=...
        find_library(LIBUV_LIBRARY NAMES uv libuv)
        if(LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
            add_library(agar_libuv UNKNOWN IMPORTED)
            set_target_properties(agar_libuv PROPERTIES
                IMPORTED_LOCATION "${LIBUV_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_DIR}")
            set(AGAR_LIBUV_TARGET agar_libuv)
        endif()
    endif()

    if(NOT AGAR_LIBUV_TARGET)
        message(STATUS "libuv не найдена в системе - скачиваю и собираю v1.48.0")
        include(FetchContent)
        set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(libuv
            GIT_REPOSITORY https://github.com/libuv/libuv.git
            GIT_TAG v1.48.0)
        FetchContent_MakeAvailable(libuv)
        set(AGAR_LIBUV_TARGET uv_a)
    endif()

    add_executable(agar_server server/main.cpp server/server.cpp)
    target_include_directories(agar_server PRIVATE server)
    target_link_libraries(agar_server PRIVATE agar_common ${AGAR_LIBUV_TARGET})
endif()

# ---------------------------------------------------------------- клиент (SFML)
if(AGAR_BUILD_CLIENT)
    find_package(SFML 2.5 COMPONENTS graphics window network system QUIET)
    if(NOT SFML_FOUND)
        message(STATUS "SFML 2.x не найдена в системе - скачиваю и собираю 2.6.1")
        include(FetchContent)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SFML
            GIT_REPOSITORY https://github.com/SFML/SFML.git
            GIT_TAG 2.6.1)
        FetchContent_MakeAvailable(SFML)
    endif()

    add_executable(agar_client
        client/main.cpp
        client/app.cpp
        client/net.cpp
        client/skins.cpp
        client/menu_screen.cpp
        client/create_player_screen.cpp
        client/game_screen.cpp
        client/records_screen.cpp)
    target_include_directories(agar_client PRIVATE client)
    target_link_libraries(agar_client PRIVATE agar_common
        sfml-graphics sfml-window sfml-network sfml-system)
    target_compile_definitions(agar_client PRIVATE AGAR_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

    # Ресурсы (шрифт, при желании road.png) кладём рядом с исполняемым файлом.
    add_custom_command(TARGET agar_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:agar_client>/assets)
endif()

# ---------------------------------------------------------------- тесты
if(AGAR_BUILD_TESTS)
    enable_testing()
    add_executable(agar_tests tests/world_test.cpp)
    target_link_libraries(agar_tests PRIVATE agar_common)
    add_test(NAME agar_tests COMMAND agar_tests)
endif()
