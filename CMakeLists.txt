cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)

project(WeilanEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

if (MSVC)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
endif()

option(USE_VULKAN_SDK_TOOLS "Turn it off if want to build our own vulkan sdk tools, useful when we want to build in Debug build type" OFF)
option(ENABLE_ADDRESS_SANITIZER "" OFF)

include(CheckIPOSupported)
include(FetchContent)

if (ENABLE_ADDRESS_SANITIZER)
add_compile_options("/fsanitize=address")
endif()

FetchContent_Declare(Slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    GIT_TAG "v2025.6.3"
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
)
set(SLANG_ENABLE_DXIL OFF)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_ENABLE_SLANGD OFF)
set(SLANG_ENABLE_SLANGC OFF)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_RELEASE_DEBUG_INFO OFF)
set(SLANG_ENABLE_SLANG_RHI OFF)
set(SLANG_ENABLE_CUDA OFF)
set(SLANG_ENABLE_OPTIX OFF)
set(SLANG_ENABLE_NVAPI OFF)
set(SLANG_ENABLE_AFTERMATH OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
FetchContent_MakeAvailable(Slang)

add_dependencies(slang
slang-glslang
)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG "release-2.30.x"
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )

if (ENABLE_ADDRESS_SANITIZER)
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
endif()

FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.1.3
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )
set(FMT_DOC OFF)
set(FMT_TEST OFF)
set(FMT_SYSTEM_HEADERS ON)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(SPDLOG
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
set(SPDLOG_BUILD_EXAMPLE OFF)
set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(SPDLOG)

FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG v0.7.2
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_MakeAvailable(rapidyaml)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
set(JSON_BuildTests OFF)
FetchContent_MakeAvailable(json)

FetchContent_Declare(glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG 1.0.1
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_MakeAvailable(glm)

FetchContent_Declare(vma
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "v3.1.0"
    UPDATE_DISCONNECTED TRUE
    )
set(VMA_ENABLE_INSTALL OFF)
FetchContent_MakeAvailable(vma)

FetchContent_Declare(ktx_software
    GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
    GIT_TAG v4.3.1
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )
set(BASISU_SUPPORT_SSE "${BASISU_SUPPORT_SSE}")
set(KTX_FEATURE_TESTS OFF)
FetchContent_MakeAvailable(ktx_software)


FetchContent_Declare(
        assimp
        GIT_REPOSITORY "https://github.com/assimp/assimp.git"
        GIT_TAG "v5.4.3"
        UPDATE_DISCONNECTED TRUE
        SOURCE_SUBDIR "assimp"
)
if(NOT assimp_POPULATED)
    FetchContent_MakeAvailable(assimp)
    set(ASSIMP_BUILD_TESTS OFF)
    set(ASSIMP_INSTALL OFF)
    set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
    add_subdirectory(${assimp_SOURCE_DIR})
endif()

FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG "v5.2.0"
        UPDATE_DISCONNECTED TRUE
        SOURCE_SUBDIR "Jolt"
)
check_ipo_supported(RESULT IS_IPO_SUPPORTED)
set(DOUBLE_PRECISION OFF)
set(GENERATE_DEBUG_SYMBOLS OFF)
set(CROSS_PLATFORM_DETERMINISTIC ON)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
set(OBJECT_LAYER_BITS 32)
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX ON)
set(USE_AVX2 ON)
set(USE_AVX512 OFF)
set(USE_LZCNT ON)
set(USE_TZCNT ON)
set(USE_F16C ON)
set(USE_FMADD ON)
# set(BUILD_SHARED_LIBS ON)
set(INTERPROCEDURAL_OPTIMIZATION=${IS_IPO_SUPPORTED})
if(NOT JoltPhysics_POPULATED)
    FetchContent_MakeAvailable(JoltPhysics)

  set(PHYSICS_REPO_ROOT ${joltphysics_SOURCE_DIR})
  include(${joltphysics_SOURCE_DIR}/Jolt/Jolt.cmake)
  target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_PROFILE_ENABLED;JPH_DEBUG_RENDERER>")
  target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:Debug>:JPH_PROFILE_ENABLED;JPH_DEBUG_RENDERER>")
  if (FLOATING_POINT_EXCEPTIONS_ENABLED AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_FLOATING_POINT_EXCEPTIONS_ENABLED>")
      target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:Debug>:JPH_FLOATING_POINT_EXCEPTIONS_ENABLED>")
  endif()
endif()


# WeilanEngine

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

option(UNIT_TEST "enable unit test" ON)
option(SHIP "ship build" OFF)
option(EDITOR_ON "compile with game editor" ON)
option(DEV_BUILD "Define ENGINE_DEV_BUILD" ON)

# link WeilanEngine to editor
add_executable(WeilanEngineEditor
    Source/Editor/Launcher.cpp)
add_library(WeilanEngine SHARED)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    target_compile_options(WeilanEngine PUBLIC /MP /W1)
    target_compile_options(WeilanEngine PRIVATE "/Zi")
    target_compile_options(WeilanEngine PRIVATE "/Od")
    target_compile_options(WeilanEngine PRIVATE "/Ob0")
    target_compile_options(WeilanEngineEditor PRIVATE "/Zi")
endif()

set_property(TARGET WeilanEngine PROPERTY INTERPROCEDURAL_OPTIMIZATION IS_IPO_SUPPORTED)
file(GLOB_RECURSE CORE_ENGINE_SRC "Source/Engine/*.c" "Source/Engine/*.cpp" "Source/Engine/*.hpp" "Source/Engine/*.h" "Source/Engine/*.tpp")
if (${DEV_BUILD})
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_DEV_BUILD)
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_SOURCE_PATH="${CMAKE_SOURCE_DIR}")
endif()

if (${EDITOR_ON})
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_EDITOR)
endif()

target_sources(WeilanEngine PRIVATE ${CORE_ENGINE_SRC})
find_package(Vulkan REQUIRED)

target_include_directories( WeilanEngine
    PUBLIC
    "Source/Engine/"

    SYSTEM
    ${Vulkan_INCLUDE_DIRS}
    )

if(USE_VULKAN_SDK_TOOLS)
    target_link_directories(WeilanEngine
        PUBLIC
        $ENV{VULKAN_SDK}/Lib
        )
endif()

target_link_libraries( WeilanEngine
    PUBLIC
    ryml::ryml
    SDL2::SDL2
    spdlog
    fmt
    ktx
    glm
    slang
    fmt
    ktx
    glm
    Jolt
    nlohmann_json::nlohmann_json
    GPUOpen::VulkanMemoryAllocator
    assimp
    ${Vulkan_LIBRARIES}
    )

target_compile_definitions(
    WeilanEngine
    PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
    SPDLOG_FMT_EXTERNAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    VULKAN_HPP_CPP_VERSION=20
    GLM_ENABLE_EXPERIMENTAL
    )

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    target_compile_options(WeilanEngineEditor PRIVATE "/Zi")
    target_compile_options(WeilanEngineEditor PRIVATE "/Od")
    target_compile_options(WeilanEngineEditor PRIVATE "/Ob0")
endif()
file(GLOB_RECURSE EDITOR_ENGINE_SRC "Source/Editor/*.c" "Source/Editor/*.cpp" "Source/Editor/*.hpp" "Source/Editor/*.h" "Source/Editor/*.tpp")
target_sources(WeilanEngineEditor PRIVATE ${EDITOR_ENGINE_SRC})
target_link_libraries(WeilanEngineEditor WeilanEngine)
target_include_directories( WeilanEngineEditor
    PUBLIC
    "Source/Editor/"

    SYSTEM
    # "${CMAKE_BINARY_DIR}/bin"
    # "${CMAKE_BINARY_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
    )

if (MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "WeilanEngineEditor")
    set_property(TARGET WeilanEngineEditor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

source_group(
    TREE
    ${CMAKE_CURRENT_LIST_DIR}
    FILES
    ${CORE_ENGINE_SRC}
    ${Dev_Tool_SRC}
)

if (UNIT_TEST)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    file(GLOB_RECURSE CORE_TEST_ENGINE_SRC "./Source/Test/*.c" "./Source/Test/*.cpp" "./Source/Test/*.hpp" "./Source/Test/*.h" "./Source/Test/*.tpp")
    add_executable(
        WeilanEngineTest
        ${CORE_TEST_ENGINE_SRC}
    )
    target_link_libraries(
        WeilanEngineTest
        WeilanEngine
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(WeilanEngineTest)
endif()
