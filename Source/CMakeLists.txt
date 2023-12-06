# WeilanEngine
cmake_minimum_required(VERSION 3.27.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
#     set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin" )
#     set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin" )
#     set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin" )
# endif()

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
target_link_libraries(WeilanEngineEditor -framework WeilanEngine)

find_package(Vulkan REQUIRED)

if (${is_ios})
    find_library(
        MoltenVK_LIBRARY
        MoltenVK
        HINTS
        $ENV{VULKAN_SDK}/../MoltenVK/dylib/iOS
        )
    set(VULKAN_LIBS ${MoltenVK_LIBRARY})
else()
    set(VULKAN_LIBS ${Vulkan_LIBRARIES})
endif()

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

if("${CMAKE_GENERATOR}" MATCHES "(Xcode)|(Visual Studio).+")
    if (NOT ${is_ios})
        SET(SDL2_LIB_NAME sdl2$<$<CONFIG:Debug>:d>)
    else()
        find_library(SDL2_LIB_NAME 
            SDL2
            HINTS 
            "${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/SDL2/Xcode/SDL/build/Debug-iphoneos"
            NO_DEFAULT_PATH
            )
    endif()
    SET(fmt_LIB_NAME fmt$<$<CONFIG:Debug>:d>)
    SET(spdlog_LIB_NAME spdlog$<$<CONFIG:Debug>:d>)
else()
    SET(SDL2_LIB_NAME sdl2)
    SET(fmt_LIB_NAME fmt)
    SET(spdlog_LIB_NAME spdlog)
endif()

# set(CMAKE_OSX_SYSROOT "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneOS.sdk")

if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    SET(APPLE_PLATFORM_SDK_PATH /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk/System/Library/Frameworks)
    find_library(CoreAudio_LIBRARY 
        CoreAudio 
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH
        )
    find_library(AudioToolBox_LIBRARY AudioToolBox
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH
        )
    find_library(Carbon_LIBRARY Carbon
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH
        )
    find_library(CoreHaptics_LIBRARY CoreHaptics
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH
        )
    find_library(CoreVideo_LIBRARY CoreVideo
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(ForceFeedback_LIBRARY ForceFeedback
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(GameController_LIBRARY GameController
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(IOKit_LIBRARY IOKit
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(Metal_LIBRARY Metal
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(QuartzCore_LIBRARY QuartzCore
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(CoreFoundation_LIBRARY CoreFoundation
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(AppKit_LIBRARY AppKit
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)
    find_library(iconv_LIBRARY iconv
        PATHS 
        ${APPLE_PLATFORM_SDK_PATH}
        NO_DEFAULT_PATH)

    target_link_libraries(WeilanEngine
        ${AudioToolBox_LIBRARY}
        ${CoreAudio_LIBRARY}
        #${Carbon_LIBRARY}
        ${CoreFoundation_LIBRARY}
        ${CoreHaptics_LIBRARY}
        ${CoreVideo_LIBRARY}
        #${ForceFeedback_LIBRARY}
        ${GameController_LIBRARY}
        ${IOKit_LIBRARY}
        ${Metal_LIBRARY}
        ${QuartzCore_LIBRARY}
        #${AppKit_LIBRARY}
        #${iconv_LIBRARY}
        )
endif()

if (${is_ios})
    set (EMBED_FRAMEWORKS
        WeilanEngine
        ${MoltenVK_LIBRARY}
        ${SDL2_LIB_NAME}
        )

    set_target_properties(WeilanEngine PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION 0.1
        MACOSX_FRAMEWORK_IDENTIFIER com.weilan.WeilanEngine
        # MACOSX_FRAMEWORK_INFO_PLIST Info.plist
        # "current version" in semantic format in Mach-O binary file
        VERSION 0.0.1
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY True
        XCODE_EMBED_FRAMEWORKS 
            "${DEPENDENCY_INSTALL_DIR}/lib/libshaderc_shared.1.dylib;${DEPENDENCY_INSTALL_DIR}/lib/libktx.4.dylib"
        # "compatibility version" in semantic format in Mach-O binary file
        SOVERSION 1.0.0
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS
        "@executable_path/Frameworks"
        )


    set (EMBED_RPATH
        "@executable_path/Frameworks @executable_path/Frameworks/WeilanEngine.framework/Frameworks"
        )
    set_target_properties(WeilanEngineEditor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME WeilanGame
        MACOSX_BUNDLE_GUI_IDENTIFIER com.weilan.WeilanGame
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY True
        XCODE_EMBED_FRAMEWORKS "${EMBED_FRAMEWORKS}"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "${EMBED_RPATH}"
        )
endif()


target_link_libraries( WeilanEngine
    # assimp
    ${VULKAN_LIBS}
    ${PHYSX_LIBS}
    ${SDL2_LIB_NAME}
    ${fmt_LIB_NAME}
    ${spdlog_LIB_NAME}
    spirv-cross-core$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-reflect$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
    spirv-cross-glsl$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:d>
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
    PRIVATE
    LUA_USE_IOS # only for iOS
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
