cmake_minimum_required(VERSION 3.20)
project(pboard)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib 5.5 REQUIRED)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE 3rd/asio/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

set(UUID_SYSTEM_GENERATOR ON CACHE BOOL "Enable system uuid generator" FORCE)
add_subdirectory(3rd/stduuid)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(SPACE_MONO_FONT "${CMAKE_CURRENT_SOURCE_DIR}/res/space-mono/SpaceMono-Regular.ttf")
set(SPACE_MONO_OUTPUT "${GENERATED_DIR}/space-mono.h")

find_program(XXD_EXECUTABLE xxd REQUIRED)

add_custom_command(
  OUTPUT "${SPACE_MONO_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
  COMMAND "${XXD_EXECUTABLE}" -i -n space_mono "${SPACE_MONO_FONT}" "${SPACE_MONO_OUTPUT}"
  DEPENDS "${SPACE_MONO_FONT}"
  COMMENT "Generating C array from font asset..."
  VERBATIM
)

set(ICONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/icons")
set(ICONS_LIST
  "${ICONS_DIR}/pan.png"
  "${ICONS_DIR}/line.png"
  "${ICONS_DIR}/rect.png"
  "${ICONS_DIR}/circle.png"
  "${ICONS_DIR}/select.png"
  "${ICONS_DIR}/bezier.png")

find_program(OBJCOPY_EXECUTABLE objcopy REQUIRED)

set(ICON_OBJECTS "")
foreach(ICON_PATH ${ICONS_LIST})
  get_filename_component(ICON_FILENAME "${ICON_PATH}" NAME)
  get_filename_component(ICON_NAME "${ICON_PATH}" NAME_WE)
  string(MAKE_C_IDENTIFIER "${ICON_FILENAME}" ICON_C_ID)

  set(ICON_OBJ "${GENERATED_DIR}/icons/${ICON_NAME}.o")

  add_custom_command(
    OUTPUT "${ICON_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}/icons"
    COMMAND "${OBJCOPY_EXECUTABLE}" -I binary -O elf64-x86-64 -B i386:x86-64
            --redefine-sym "_binary_${ICON_C_ID}_start=pboard_icons_${ICON_NAME}_start"
            --redefine-sym "_binary_${ICON_C_ID}_end=pboard_icons_${ICON_NAME}_end"
            --redefine-sym "_binary_${ICON_C_ID}_size=pboard_icons_${ICON_NAME}_size"
            "${ICON_FILENAME}" "${ICON_OBJ}"
    WORKING_DIRECTORY "${ICONS_DIR}"
    DEPENDS "${ICON_PATH}"
    COMMENT "Embedding icon ${ICON_PATH}"
    VERBATIM
  )

  list(APPEND ICON_OBJECTS "${ICON_OBJ}")
endforeach()

add_executable(pboard
  src/main.cpp
  src/board_ui.cpp
  src/net.cpp
  src/whiteboard.cpp
  src/menu.cpp
  src/protocol.cpp
  src/widget.cpp
  src/tool.cpp
  "${SPACE_MONO_OUTPUT}"
  ${ICON_OBJECTS})

target_compile_features(pboard PUBLIC cxx_std_23)
target_include_directories(pboard PRIVATE include/ "${GENERATED_DIR}")
target_compile_options(pboard PRIVATE -Wall -Wextra)
target_compile_options(pboard PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>
)
target_link_libraries(pboard PRIVATE raylib asio uuid stduuid)
target_link_options(pboard PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
