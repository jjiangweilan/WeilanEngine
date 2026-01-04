cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW) # honors GLM_BUILD_INSTALL

project(WeilanEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# Enable folder organization in Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
add_compile_options(/Zc:__cplusplus /MP)

if (ENABLE_ASAN)
add_compile_options("/fsanitize=address")
endif()

endif()

option(USE_VULKAN_SDK_TOOLS "Turn it off if want to build our own vulkan sdk tools, useful when we want to build in Debug build type" OFF)
option(ENABLE_ASAN "" OFF)

include(CheckIPOSupported)
include(FetchContent)


function(ResolveTargetAlias target resolved)
    get_target_property(aliased ${target} ALIASED_TARGET)
    if(NOT aliased STREQUAL "aliased-NOTFOUND")
        set(${resolved} ${aliased} PARENT_SCOPE)
    else()
        set(${resolved} ${target} PARENT_SCOPE)
    endif()
endfunction()

function(GetTargetDependenciesInternal inTarget outDependencies)
    set(dependencies)

    ResolveTargetAlias(${inTarget} target)

    # Get direct link libraries
    get_target_property(link_libs ${target} LINK_LIBRARIES)
    if(link_libs)
        foreach(lib ${link_libs})
            if(TARGET ${lib})
                get_target_property(aliased ${lib} ALIASED_TARGET)
                if(NOT aliased STREQUAL "aliased-NOTFOUND")
                    set(resolved_lib ${aliased})
                else()
                    set(resolved_lib ${lib})
                endif()

                list(APPEND dependencies ${resolved_lib})
                # Recursively get dependencies of this target
                GetTargetDependenciesInternal(${resolved_lib} sub_deps)
                list(APPEND dependencies ${sub_deps})
            endif()
        endforeach()
    endif()
    
    # Get interface link libraries
    get_target_property(interface_libs ${target} INTERFACE_LINK_LIBRARIES)
    if(interface_libs)
        foreach(lib ${interface_libs})
            if(TARGET ${lib})
                get_target_property(aliased ${lib} ALIASED_TARGET)
                if(NOT aliased STREQUAL "aliased-NOTFOUND")
                    set(resolved_lib ${aliased})
                else()
                    set(resolved_lib ${lib})
                endif()

                list(APPEND dependencies ${resolved_lib})
                GetTargetDependenciesInternal(${resolved_lib} sub_deps)
                list(APPEND dependencies ${sub_deps})
            endif()
        endforeach()
    endif()
    
    # Remove duplicates
    list(REMOVE_DUPLICATES dependencies)

    set(${outDependencies} ${dependencies} PARENT_SCOPE)
endfunction()

function(GetTargetDependencies inTarget outDependencies)
    set(dependencies)

    ResolveTargetAlias(${inTarget} target)

    list(APPEND dependencies ${target})
    GetTargetDependenciesInternal(${target} sub_deps)
    list(APPEND dependencies ${sub_deps})
    set(${outDependencies} ${dependencies} PARENT_SCOPE)
endfunction()

function(SetDependencyFolder inTarget folder_name)
    ResolveTargetAlias(${inTarget} target)

    GetTargetDependencies(${target} dependencies)

    foreach(target ${dependencies})
        set_target_properties(${target} PROPERTIES FOLDER "Dependencies/${folder_name}")
    endforeach(target ${dependencies})
endfunction()

FetchContent_Declare(Slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    GIT_TAG "v2025.24"
    UPDATE_DISCONNECTED TRUE
    GIT_PROGRESS TRUE
)
set(SLANG_ENABLE_DXIL OFF)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_ENABLE_SLANGD OFF)
set(SLANG_ENABLE_SLANGI OFF)
set(SLANG_ENABLE_SLANGC OFF)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_RELEASE_DEBUG_INFO OFF)
set(SLANG_ENABLE_SLANG_RHI OFF)
set(SLANG_ENABLE_CUDA OFF)
set(SLANG_ENABLE_OPTIX OFF)
set(SLANG_ENABLE_NVAPI OFF)
set(SLANG_ENABLE_AFTERMATH OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_SLANG_GLSLANG ON)
set(SLANG_EMBED_CORE_MODULE_SOURCE OFF)
if(ENABLE_ASAN)
    set(SLANG_ENABLE_ASAN ON)
endif()
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

if (ENABLE_ASAN)
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
    set(SDL_STATIC OFF)
endif()
set(SDL2_DISABLE_SDL2MAIN ON)
set(SDL_TEST OFF)
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
    # GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_REPOSITORY https://github.com/jjiangweilan/rapidyaml.git # temp fix a problem where clang in windows doesn't have alloca.h header https://github.com/biojppm/rapidyaml/issues/539
    GIT_TAG v0.9.0_patch
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE
    )
FetchContent_MakeAvailable(rapidyaml)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
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
set(GLM_BUILD_TESTS OFF)
set(GLM_BUILD_INSTALL OFF)
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
set(KTX_FEATURE_TOOLS OFF)
FetchContent_MakeAvailable(ktx_software)

set(BOOST_INCLUDE_LIBRARIES unordered;lockfree;program_options)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.89.0
)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(
        assimp
        GIT_REPOSITORY "https://github.com/assimp/assimp.git"
        GIT_TAG "v6.0.2"
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
add_executable(WeilanEngineEditor)
add_library(WeilanEngine SHARED)
target_link_libraries(WeilanEngineEditor PRIVATE WeilanEngine)
target_include_directories(WeilanEngineEditor PRIVATE "Source/")

if (${DEV_BUILD})
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_DEV_BUILD)
    target_compile_definitions(WeilanEngine PUBLIC ENGINE_SOURCE_PATH="${CMAKE_SOURCE_DIR}")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    target_compile_options(WeilanEngine PUBLIC /W1)

if (NOT ENABLE_ASAN)
    target_compile_options(WeilanEngine PUBLIC "/ZI")
endif()
    target_compile_options(WeilanEngine PUBLIC "/Od")
    target_compile_options(WeilanEngine PUBLIC "/Ob0")
endif()

set_property(TARGET WeilanEngine PROPERTY INTERPROCEDURAL_OPTIMIZATION IS_IPO_SUPPORTED)
file(GLOB_RECURSE CORE_ENGINE_SRC "Source/Engine/*.c" "Source/Engine/*.cpp" "Source/Engine/*.hpp" "Source/Engine/*.h" "Source/Engine/*.tpp")

target_sources(WeilanEngine PRIVATE ${CORE_ENGINE_SRC})
find_package(Vulkan REQUIRED)

target_include_directories( WeilanEngine
    PRIVATE
    "Source/"

    SYSTEM
    ${Vulkan_INCLUDE_DIRS}
    )

if(USE_VULKAN_SDK_TOOLS)
    target_link_directories(WeilanEngine
        PRIVATE
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
    Jolt
    Boost::unordered
    Boost::lockfree
    Boost::program_options
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

find_package(Python3 REQUIRED)

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import tree_sitter; import tree_sitter_cpp"
    RESULT_VARIABLE TREE_SITTER_AVAILABLE
    ERROR_QUIET
)

if (NOT TREE_SITTER_AVAILABLE EQUAL 0)
    message(FATAL_ERROR "Python module 'tree_sitter' or 'tree_sitter_cpp' not found. Please install it via 'pip install tree-sitter tree-sitter-cpp'")
endif()

add_custom_target(CodeGeneration
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Source/Scripts/CodeGeneration.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Code Generation"
)

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/Source/Lua/weilan_api.lua"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Source/Scripts/GenerateLuaAnnotation.py"
    DEPENDS "${CMAKE_SOURCE_DIR}/Source/Engine/CodeGen/LuaBindings_Generated.cpp"
            "${CMAKE_SOURCE_DIR}/Source/Engine/Runtime/System/ScriptingBackend/LuaBindings.cpp"
            "${CMAKE_SOURCE_DIR}/Source/Scripts/GenerateLuaAnnotation.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating Lua Annotations"
)

add_custom_target(LuaAnnotationGen DEPENDS "${CMAKE_SOURCE_DIR}/Source/Lua/weilan_api.lua")
add_dependencies(LuaAnnotationGen CodeGeneration)

# Shader Compilation
file(GLOB_RECURSE SHADER_SOURCES 
    "${CMAKE_SOURCE_DIR}/Source/Engine/Shaders/*.slang"
    "${CMAKE_SOURCE_DIR}/Source/Engine/Shaders/*.hlsl"
)

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/Source/CompiledShader/shader_manifest.json"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Source/Scripts/CompileShaders.py"
    DEPENDS ${SHADER_SOURCES}
            "${CMAKE_SOURCE_DIR}/Source/Scripts/CompileShaders.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Compiling Shaders"
)

# ShaderCompilerTool - standalone shader compiler
add_subdirectory(Source/Tools/ShaderCompiler)

add_custom_target(CompileShaders DEPENDS "${CMAKE_SOURCE_DIR}/Source/CompiledShader/shader_manifest.json")
add_dependencies(CompileShaders ShaderCompilerTool)  # Ensure ShaderCompilerTool is built first

add_dependencies(WeilanEngine CodeGeneration)
add_dependencies(WeilanEngine LuaAnnotationGen)
add_dependencies(WeilanEngine CompileShaders)

if (${EDITOR_ON})
    target_sources(WeilanEngineEditor PRIVATE Source/Main/EditorLauncher.cpp)
    target_compile_definitions(WeilanEngine PRIVATE ENGINE_EDITOR)
    file(GLOB_RECURSE EDITOR_ENGINE_SRC "Source/Editor/*.c" "Source/Editor/*.cpp" "Source/Editor/*.hpp" "Source/Editor/*.h" "Source/Editor/*.tpp")
    target_sources(WeilanEngine PRIVATE ${EDITOR_ENGINE_SRC})
endif()

if (MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "WeilanEngineEditor")
    #    set_property(TARGET WeilanEngineEditor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
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
    target_include_directories(
        WeilanEngineTest
        PRIVATE
        "Source/"
    )

    include(GoogleTest)
    gtest_discover_tests(WeilanEngineTest)

    SetDependencyFolder(gtest_main GoogleTest)
    SetDependencyFolder(gmock_main GoogleTest)
    SetDependencyFolder(gmock GoogleTest)
endif()

SetDependencyFolder(slang Slang)
# SetDependencyFolder(slang-glslang Slang)
SetDependencyFolder(ryml::ryml ryml)
SetDependencyFolder(SDL2::SDL2 SDL2)
SetDependencyFolder(sdl_headers_copy SDL2)
SetDependencyFolder(spdlog spdlog)
SetDependencyFolder(fmt fmt)
SetDependencyFolder(ktx ktx)
SetDependencyFolder(ktx_read ktx) # not used yet, but good for runtime
SetDependencyFolder(glm glm)
SetDependencyFolder(nlohmann_json nlohmann_json)
SetDependencyFolder(VulkanMemoryAllocator VulkanMemoryAllocator)
SetDependencyFolder(assimp assimp)
SetDependencyFolder(Jolt JoltPhysics)
