cmake_minimum_required(VERSION 3.22.1)
project(pmlxzj_unlocker VERSION 0.1.7)
cmake_policy(SET CMP0135 NEW)

option(STATIC_ZLIB "static link zlib" OFF)

set(CMAKE_C_STANDARD 17)

include(FetchContent)
FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
)
FetchContent_MakeAvailable(zlib)

string(CONCAT pmlxzj_unlocker_ver
        ${pmlxzj_unlocker_VERSION_MAJOR} "."
        ${pmlxzj_unlocker_VERSION_MINOR} "."
        ${pmlxzj_unlocker_VERSION_PATCH})

add_executable(pmlxzj_unlocker
        main.c
        pmlxzj.c
        pmlxzj_audio.c
        pmlxzj_frame.c
        cmd_disable_audio.c
        cmd_extract_audio.c
        cmd_info.c
        cmd_unlock_exe.c
)
target_compile_definitions(pmlxzj_unlocker PRIVATE PROJECT_VERSION="${pmlxzj_unlocker_ver}")

if (STATIC_ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    message("using static zlib")
else ()
    add_library(ZLIB::ZLIB ALIAS zlib)
    message("using dynamic zlib")

    add_custom_command(TARGET pmlxzj_unlocker POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zlib>
            $<TARGET_FILE_DIR:pmlxzj_unlocker>)
endif ()

target_link_libraries(pmlxzj_unlocker ZLIB::ZLIB)

if (MSVC)
    target_compile_options(pmlxzj_unlocker PRIVATE /W4 /WX)
else ()
    target_compile_options(pmlxzj_unlocker PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(pmlxzj_unlocker PRIVATE -static-libgcc -Wl,--gc-sections)
    endif ()
endif ()

if (WIN32)
    target_link_libraries(pmlxzj_unlocker shell32.lib)
endif ()
