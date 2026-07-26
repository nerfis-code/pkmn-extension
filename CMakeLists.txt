cmake_minimum_required(VERSION 3.17)

project("PKMN-EXTENSION")

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "PKMN-EXTENSION")
set(GODOT_PROJECT_DIR "project" CACHE STRING "The directory of a Godot project folder")

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_subdirectory(third_party/godot-cpp SYSTEM)
add_subdirectory(third_party/quickjs SYSTEM)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/gen/include")

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

project(pokedot-expansion
    VERSION 1.0
    DESCRIPTION "This repository serves as a quickstart template for GDExtension development with Godot 4.0+."
    HOMEPAGE_URL "https://github.com/enetheru/godot-cpp-template/tree/main"
    LANGUAGES CXX C
)

set(BUNDLED_JS ${CMAKE_CURRENT_BINARY_DIR}/bundle.js)
set(GENERATED_C ${CMAKE_CURRENT_BINARY_DIR}/out.c)

add_custom_command(
  OUTPUT  ${BUNDLED_JS}
  COMMAND npx.cmd --yes esbuild ${CMAKE_CURRENT_SOURCE_DIR}/sim/main.js --bundle --format=esm --outfile=${BUNDLED_JS}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/main.js
  COMMENT "Bundling index.js con esbuild"
  VERBATIM
)

add_custom_command(
  OUTPUT  ${GENERATED_C}
  COMMAND qjsc -N qjsc_out -m -o ${GENERATED_C} ${BUNDLED_JS}
  DEPENDS qjsc ${BUNDLED_JS}
  COMMENT "Compilando bundle.js a bytecode C (out.c) con qjsc"
  VERBATIM
)

add_custom_target(generate_bytecode DEPENDS ${GENERATED_C})

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/pbattle.cpp
    src/pserver.cpp
    ${GENERATED_C}
)

add_dependencies(${LIBNAME} generate_bytecode)

target_link_libraries(${LIBNAME} PRIVATE godot-cpp qjs qjs-libc nlohmann_json::nlohmann_json)

set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 23)

set_target_properties(${LIBNAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"
    PREFIX ""
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/bin/${GODOTCPP_PLATFORM}")

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>" "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
)