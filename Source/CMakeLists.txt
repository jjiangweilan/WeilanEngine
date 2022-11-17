# WeilanEngine
cmake_minimum_required(VERSION 3.18.0)


option(SHIP "ship build" OFF)
option(EDITOR_ON "compile with game editor" ON)
option(DEV_BUILD "use internal asset inside the source tree instead of installed location" ON)

add_executable(WeilanEngine
    Engine/Launcher.cpp)
add_library(WeilanEngineLib)


target_link_libraries(WeilanEngine 
    WeilanEngineLib)

file(GLOB_RECURSE CORE_ENGINE_SRC "Engine/*.c" "Engine/*.cpp" "Engine/*.hpp" "Engine/*.h" "Engine/*.tpp")

if (${DEV_BUILD})
    target_compile_definitions(WeilanEngineLib PUBLIC ENGINE_DEV_BUILD)
    target_compile_definitions(WeilanEngineLib PUBLIC ENGINE_SOURCE_PATH="${CMAKE_SOURCE_DIR}")
endif()

if (${EDITOR_ON})
    target_compile_definitions(WeilanEngineLib PUBLIC GAME_EDITOR)
else()
    list(FILTER CORE_ENGINE_SRC EXCLUDE REGEX "${CMAKE_CURRENT_LIST_DIR}/Engine/Editor/*")
endif()

target_sources(WeilanEngineLib PRIVATE ${CORE_ENGINE_SRC})

find_package(Vulkan REQUIRED FATAL_ERROR)

target_include_directories( WeilanEngineLib
    PUBLIC
    "Engine/"

    SYSTEM
    "${CMAKE_BINARY_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
)

if (INSTALL_DEPENDENCY)
    add_dependencies(WeilanEngineLib ${DEPEND_NAMES})
endif()

target_link_directories(
    WeilanEngineLib
    PUBLIC 
    "${CMAKE_BINARY_DIR}/lib"
)

target_link_libraries( WeilanEngineLib
    # assimp
    ${ClangMinGWLibs}
    ${Vulkan_LIBRARIES}
    sdl2$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spdlog$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-core$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-reflect$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-glsl$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    shaderc_shared
    ryml
    )

target_compile_definitions(
    WeilanEngineLib
    PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if (MSVC)
    target_compile_options(WeilanEngineLib PUBLIC /MP /W0) # currently we have so many compiler warnings. I shut them down for now
endif()

if (NOT SHIP)

file(GLOB_RECURSE Dev_Tool_SRC "Editor/*.c" "Editor/*.cpp" "Editor/*.hpp" "Editor/*.h" "Editor/*.tpp")
file(GLOB_RECURSE Dev_Tool_INCLUDE "Editor/*.hpp" "Editor/*.h" "Editor/*.tpp")
target_sources(WeilanEngineLib 
    PRIVATE ${Dev_Tool_SRC}
    PRIVATE ${Dev_Tool_INCLUDE}
    )
    
target_include_directories(WeilanEngineLib
    PUBLIC
    "./Editor"
)

# add_subdirectory("Editor/")
add_compile_definitions(NSHIP)
# target_link_libraries(
#     WeilanEngineLib
#     DevTool
# )
# # target_include_directories(
# #     WeilanEngineLib
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
#add_dependencies(WeilanEngineLib copyAsset)

# move builtin assets files
if (WIN32)
    set(BuiltInAssetsDir $ENV{LOCALAPPDATA}/WeilanEngine/Assets/)
elseif (APPLE)
    set(BuiltInAssetsDir $ENV{HOME}/.config/WeilanEngine/Assets/)
endif()
add_custom_target(copyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_LIST_DIR}/../Assets
    ${BuiltInAssetsDir}
)
add_dependencies(WeilanEngineLib copyAssets)

# Unit Test
add_subdirectory(Test/)
