cmake_minimum_required(VERSION 3.20)
project(pboard)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib 5.5 REQUIRED)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE 3rd/asio/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

set(UUID_SYSTEM_GENERATOR ON CACHE BOOL "Enable system uuid generator" FORCE)
add_subdirectory(3rd/stduuid)

add_executable(pboard src/main.cpp src/board_ui.cpp src/net.cpp src/whiteboard.cpp src/menu.cpp src/protocol.cpp)
target_compile_features(pboard PUBLIC cxx_std_23)
target_include_directories(pboard PRIVATE include/)
target_compile_options(pboard PRIVATE -Wall -Wextra)
target_compile_options(pboard PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>
)
target_link_libraries(pboard PRIVATE raylib asio uuid stduuid)
target_link_options(pboard PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
