cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)

project(WeilanEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )

option(USE_VULKAN_SDK_TOOLS "Turn it off if want to build our own vulkan sdk tools, useful when we want to build in Debug build type" OFF)

include(CheckIPOSupported)
include(FetchContent)

FetchContent_Declare(Slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    GIT_TAG "f4d5372d3354e62770b076b47892b5172223e98a"
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

set_target_properties(slang-glslang
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
set_target_properties(slang
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
add_dependencies(slang
slang-glslang
)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG "release-2.30.x"
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )
set(SDL_SHARED ON)
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

add_subdirectory(Source)
