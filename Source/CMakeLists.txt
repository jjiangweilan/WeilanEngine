# WeilanEngine
cmake_minimum_required(VERSION 3.18.0)
cmake_policy(SET CMP0114 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin" )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin" )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin" )
endif()

option(UNIT_TEST "enable unit test" ON)
option(SHIP "ship build" OFF)
option(EDITOR_ON "compile with game editor" ON)
option(DEV_BUILD "use internal asset inside the source tree instead of installed location" ON)

add_library(WeilanEngine SHARED)
add_executable(WeilanEngineEditor
    Editor/Launcher.cpp)



file(GLOB_RECURSE CORE_ENGINE_SRC "Engine/*.c" "Engine/*.cpp" "Engine/*.hpp" "Engine/*.h" "Engine/*.tpp")
file(GLOB_RECURSE EDITOR_ENGINE_SRC "Editor/*.c" "Editor/*.cpp" "Editor/*.hpp" "Editor/*.h" "Editor/*.tpp")

if (${DEV_BUILD})
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_DEV_BUILD)
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_SOURCE_PATH="${CMAKE_SOURCE_DIR}")
endif()

if (${EDITOR_ON})
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_EDITOR)
else()
    list(FILTER CORE_ENGINE_SRC EXCLUDE REGEX "${CMAKE_CURRENT_LIST_DIR}/Engine/Editor/*")
endif()

target_sources(WeilanEngine PRIVATE ${CORE_ENGINE_SRC})

# link WeilanEngine to editor
target_sources(WeilanEngineEditor PRIVATE ${EDITOR_ENGINE_SRC})
target_link_libraries(WeilanEngineEditor WeilanEngine)

find_package(Vulkan REQUIRED)

target_include_directories( WeilanEngine
    PUBLIC
    "Engine/"

    SYSTEM
    "${CMAKE_BINARY_DIR}/bin"
    "${CMAKE_BINARY_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
)

target_include_directories( WeilanEngineEditor
    PUBLIC
    "Editor/"

    SYSTEM
    "${CMAKE_BINARY_DIR}/bin"
    "${CMAKE_BINARY_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
)

if (INSTALL_DEPENDENCY)
    add_dependencies(WeilanEngine ${DEPEND_NAMES})
endif()

target_link_directories(
    WeilanEngine
    PUBLIC 
    "${CMAKE_BINARY_DIR}/lib"
)

target_link_libraries( WeilanEngine
    # assimp
    ${Vulkan_LIBRARIES}
    ${PHYSX_LIBS}
    sdl2$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spdlog$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-core$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-reflect$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-glsl$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    fmt$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    shaderc_shared
    ktx
    ryml
    )

target_compile_definitions(
    WeilanEngine
    PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
    SPDLOG_FMT_EXTERNAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if (MSVC)
    target_compile_options(WeilanEngine PUBLIC /MP /W0) # currently we have so many compiler warnings. I shut them down for now
endif()

if (NOT SHIP)

file(GLOB_RECURSE Dev_Tool_SRC "Editor/*.c" "Editor/*.cpp" "Editor/*.hpp" "Editor/*.h" "Editor/*.tpp")
file(GLOB_RECURSE Dev_Tool_INCLUDE "Editor/*.hpp" "Editor/*.h" "Editor/*.tpp")
target_sources(WeilanEngine 
    PRIVATE ${Dev_Tool_SRC}
    PRIVATE ${Dev_Tool_INCLUDE}
    )
    
target_include_directories(WeilanEngine
    PUBLIC
    "./Editor"
)

# add_subdirectory("Editor/")
add_compile_definitions(NSHIP)
# target_link_libraries(
#     WeilanEngine
#     DevTool
# )
# # target_include_directories(
# #     WeilanEngine
# #     PRIVATE
# #     "Editor/"
# # )
endif()

source_group(
    TREE
    ${CMAKE_CURRENT_LIST_DIR}
    FILES
    ${CORE_ENGINE_SRC}
    ${Dev_Tool_SRC}
)

#add_custom_target(
# spv2glsl ALL
# COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/python/GLSLProcessor.py
# COMMENT "Generating spv from glsl"
#)

#add_custom_target(
#    copyAsset ALL
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_CURRENT_LIST_DIR}/../Assets/
#        ${CMAKE_BINARY_DIR}/bin/Assets/
#    DEPENDS spv2glsl
#    COMMENT "copying assets"
#    )
#add_dependencies(WeilanEngine copyAsset)

# move builtin assets files
# if (WIN32)
#     set(BuiltInAssetsDir $ENV{LOCALAPPDATA}/WeilanEngine/Assets/)
# elseif (APPLE)
#     set(BuiltInAssetsDir $ENV{HOME}/.config/WeilanEngine/Assets/)
# endif()
# add_custom_target(copyAssets ALL
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_LIST_DIR}/../Assets
#     ${BuiltInAssetsDir}
# )
# add_dependencies(WeilanEngine copyAssets)

# Unit Test
if (UNIT_TEST)
    add_subdirectory(Test/)
endif()
