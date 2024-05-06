cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)

project(WeilanEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )

option(USE_VULKAN_SDK_TOOLS "Turn it off if want to build our own vulkan sdl tools, useful when we want to build in Debug build type" OFF)

include(CheckIPOSupported)
include(FetchContent)

if(NOT USE_VULKAN_SDK_TOOLS)
    FetchContent_Declare(SPIRV-Cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG vulkan-sdk-1.3.275.0 # match the least sdk
        UPDATE_DISCONNECTED TRUE
        GIT_PROGRESS TRUE
        )

    set(SPIRV_CROSS_CLI OFF)
    set(SPIRV_CROSS_ENABLE_TESTS OFF)
    FetchContent_MakeAvailable(SPIRV-Cross)

    FetchContent_Declare(SPIRV-Headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG 8b246ff75c6615ba4532fe4fde20f1be090c3764
        UPDATE_DISCONNECTED TRUE
        GIT_PROGRESS TRUE
        )
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
    set(SPIRV_HEADERS_SKIP_INSTALL ON)
    FetchContent_MakeAvailable(SPIRV-Headers)

    FetchContent_Declare(SPIRV-Tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG 04896c462d9f3f504c99a4698605b6524af813c1
        UPDATE_DISCONNECTED TRUE
        GIT_PROGRESS TRUE
        )
    set(SKIP_SPIRV_TOOLS_INSTALL ON)
    set(SPIRV_SKIP_EXECUTABLES ON)
    set(SPIRV_SKIP_TESTS ON)
    FetchContent_MakeAvailable(SPIRV-Tools)

    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG d73712b8f6c9047b09e99614e20d456d5ada2390
        UPDATE_DISCONNECTED TRUE
        GIT_PROGRESS TRUE
        )
    set(SKIP_GLSLANG_INSTALL ON)
    set(ENABLE_SPVREMAPPER OFF)
    set(ENABLE_GLSLANG_BINARIES OFF)
    set(ENABLE_GLSLANG_JS OFF)
    set(ENABLE_GLSLANG_BINARIES OFF)
    FetchContent_MakeAvailable(glslang)


    FetchContent_Declare(shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG v2024.0
        UPDATE_DISCONNECTED TRUE
        GIT_PROGRESS TRUE
        )
    set(SHADERC_ENABLE_WGSL_OUTPUT OFF)
    set(SHADERC_SKIP_INSTALL ON)
    set(SHADERC_SKIP_TESTS ON)
    set(SHADERC_SKIP_EXAMPLES ON)
    set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
    set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
    set(SHADERC_ENABLE_WERROR_COMPILE OFF)
    set(SHADERC_ENABLE_SHARED_CRT ON)
    FetchContent_MakeAvailable(shaderc)
endif()

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG "release-2.24.x"
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
    )
set(FMT_DOC OFF)
set(FMT_TEST OFF)
set(FMT_SYSTEM_HEADERS ON)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(SPDLOG
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.9.2
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
set(SPDLOG_BUILD_EXAMPLE OFF)
set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(SPDLOG)

FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG v0.4.1
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_MakeAvailable(rapidyaml)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.10.4
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
set(JSON_BuildTests OFF)
FetchContent_MakeAvailable(json)

FetchContent_Declare(glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG "0.9.9.8"
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_MakeAvailable(glm)

FetchContent_Declare(vma
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "0aa3989b8f382f185fdf646cc83a1d16fa31d6ab"
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_GetProperties(vma)
if(NOT vma_POPULATED)
    FetchContent_Populate(vma)
endif()

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
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG "v4.0.2"
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
  FetchContent_Populate(JoltPhysics)

  set(PHYSICS_REPO_ROOT ${joltphysics_SOURCE_DIR})
  include(${joltphysics_SOURCE_DIR}/Jolt/Jolt.cmake)
  target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_PROFILE_ENABLED;JPH_DEBUG_RENDERER>")
  if (FLOATING_POINT_EXCEPTIONS_ENABLED AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_FLOATING_POINT_EXCEPTIONS_ENABLED>")
  endif()
endif()

add_subdirectory(Source)
