cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)

project(WeilanEngine)

option(INSTALL_DEPENDENCY "install/update dependencies" OFF)

if (INSTALL_DEPENDENCY)
    include(ExternalProject)

    # ExternalProject_Add(SPIRV-Cross
    #     PREFIX "ThirdParty/SPIRV-Cross"
    #     GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Cross.git"
    #     GIT_TAG "7cb2f62e3ba4d9acff27fea40ddc5a2844f39d5e"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/SPIRV-Cross"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         "-DSPIRV_CROSS_CLI=OFF"
    #         "-DSPIRV_CROSS_ENABLE_TESTS=OFF"
    # )
    # list(APPEND DEPEND_NAMES SPIRV-Cross)

    # ExternalProject_Add(fmt_lib
    #     PREFIX "ThirdParty/fmt"
    #     GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    #     GIT_TAG "9.1.0"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/fmt"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         "-DFMT_DOC=OFF"
    #         "-DFMT_TEST=OFF"
    #         "-DFMT_SYSTEM_HEADERS=ON"
    # )
    # list(APPEND DEPEND_NAMES fmt_lib)
    
    # ExternalProject_Add(SPDLOG
    #     PREFIX "ThirdParty/spdlog"
    #     GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    #     GIT_TAG "v1.9.2"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spdlog"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     DEPENDS fmt_lib
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         "-DSPDLOG_BUILD_EXAMPLE=OFF"
    #         "-DSPDLOG_FMT_EXTERNAL=ON"
    # )
    # list(APPEND DEPEND_NAMES SPDLOG)

    # if (WIN32)
    #     set (SDL2_INSTALL_COMMAND
    #         cmake --install "<BINARY_DIR>" &&
    #         cmake -E copy
    #     )
    # endif (WIND32)

    # ExternalProject_Add(SDL2
    #     PREFIX "ThirdParty/SDL2"
    #     GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    #     GIT_TAG "release-2.24.x"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/SDL2"
    #     INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    # )
    # list(APPEND DEPEND_NAMES SDL2)

    # ExternalProject_Add(rapidyaml
    #     PREFIX "ThirdParty/rapidyaml"
    #     GIT_REPOSITORY "https://github.com/biojppm/rapidyaml.git"
    #     GIT_TAG "v0.4.1"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/rapidyaml"
    #     INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    # )
    # list(APPEND DEPEND_NAMES rapidyaml)


     # Used by spirv-tools
    #  ExternalProject_Add(spirv-headers
    #      PREFIX "ThirdParty/spirv-headers"
    #      GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
    #      GIT_TAG "8b246ff75c6615ba4532fe4fde20f1be090c3764" #from shaderc's DEPS file
    #      SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
    #      INSTALL_COMMAND ""
    #      CMAKE_ARGS 
    #          #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #          "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
    #          "-DSPIRV_HEADERS_SKIP_INSTALL=ON"
    #      #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #  )
    #
    #  # Used by shaderc
    #  ExternalProject_Add(spirv-tools
    #      PREFIX "ThirdParty/spirv-tools"
    #      GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git"
    #      GIT_TAG "04896c462d9f3f504c99a4698605b6524af813c1" #from shaderc's DEPS file
    #      SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-tools"
    #      INSTALL_COMMAND ""
    #      DEPENDS spirv-headers
    #      CMAKE_ARGS 
    #          #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #          "-DSPIRV_SKIP_EXECUTABLES=ON"
    #          "-DSKIP_SPIRV_TOOLS_INSTALL=ON"
    #          "-DSPIRV-Headers_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
    #      #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #  )
    #
    # # Used by shaderc
    # ExternalProject_Add(KHR_glslang
    #     PREFIX "ThirdParty/glslang"
    #     GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
    #     GIT_TAG "d73712b8f6c9047b09e99614e20d456d5ada2390" #from shaderc's DEPS file
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glslang"
    #     INSTALL_COMMAND ""
    #     # INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    #     DEPENDS spirv-tools
    #     CMAKE_ARGS 
    #     #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #        # "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         "-DENABLE_CTEST=OFF"
    #         "-DENABLE_GLSLANG_BINARIES=OFF"
    #         "-DALLOW_EXTERNAL_SPIRV_TOOLS=ON"
    # )
    #
    # ExternalProject_Add(shaderc
    #     PREFIX "ThirdParty/shaderc"
    #     GIT_REPOSITORY "https://github.com/google/shaderc.git"
    #     GIT_TAG "v2024.0"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/shaderc"
    #     INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    #     DEPENDS KHR_glslang spirv-headers
    #     CMAKE_ARGS 
    #     "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #     #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #     #"-DCMAKE_CXX_FLAGS=-Wno-deprecated-declarations"
    #     "-DSHADERC_SKIP_TESTS=ON"
    #     "-DSHADERC_SKIP_EXAMPLES=ON"
    #     "-DSHADERC_THIRD_PARTY_ROOT_DIR=${CMAKE_CURRENT_LIST_DIR}/ThirdParty"
    #     "-DSHADERC_ENABLE_SHARED_CRT=ON"
    #
    #     "-DENABLE_CTEST=OFF"
    #     "-DENABLE_GLSLANG_BINARIES=OFF"
    #     "-DSPIRV_SKIP_EXECUTABLES=ON"
    #     "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
    #     )
    # list(APPEND DEPEND_NAMES shaderc)
    #
    #
    # ExternalProject_Add(ASSIMP
    #     PREFIX "ThirdParty/assimp"
    #     GIT_REPOSITORY "https://github.com/assimp/assimp.git"
    #     GIT_TAG "v5.2.5"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/assimp"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         # "-DCMAKE_CXX_FLAGS=-Wno-deprecated-declarations"
    #         "-DBUILD_SHARED_LIBS=ON"
    #         "-DASSIMP_BUILD_TESTS=OFF"
    #         # "-DASSIMP_BUILD_ZLIB=ON"
    #         "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF"
    #         "-DASSIMP_INSTALL_PDB=OFF"
    # )
# list(APPEND DEPEND_NAMES ASSIMP)

    # ExternalProject_Add(ktx_software
    #     PREFIX "ThirdParty/ktx"
    #     GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
    #     GIT_TAG "adcccf152861158373311930e9b5e237d19168db"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/ktx_software"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS
    #     "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
    #     "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #     "-DBASISU_SUPPORT_SSE=${BASISU_SUPPORT_SSE}"
    #     "-DKTX_FEATURE_TESTS=OFF"
    #     )
    # list(APPEND DEPEND_NAMES ktx_software)

    ExternalProject_Add(googletest
        PREFIX "ThirdParty/googletest"
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "0320f517fd920866d918e564105d68fd4362040a"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/googletest"
        INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/gtest"
        CMAKE_ARGS 
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-Dgtest_force_shared_crt=ON"
        )
endif()

include (FetchContent)

FetchContent_Declare(SPIRV-Cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG 7cb2f62e3ba4d9acff27fea40ddc5a2844f39d5e
        GIT_PROGRESS TRUE
        )

set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
FetchContent_MakeAvailable(SPIRV-Cross)




FetchContent_Declare(SPIRV-Headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG 8b246ff75c6615ba4532fe4fde20f1be090c3764
        GIT_PROGRESS TRUE
        )
set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
set(SPIRV_HEADERS_SKIP_INSTALL ON)
FetchContent_MakeAvailable(SPIRV-Headers)

FetchContent_Declare(SPIRV-Tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG 04896c462d9f3f504c99a4698605b6524af813c1
        GIT_PROGRESS TRUE
        )
set(SKIP_SPIRV_TOOLS_INSTALL ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_SKIP_TESTS ON)
FetchContent_MakeAvailable(SPIRV-Tools)

FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG d73712b8f6c9047b09e99614e20d456d5ada2390
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
        GIT_PROGRESS TRUE
        )
set(SHADERC_ENABLE_WGSL_OUTPUT OFF)
set(SHADERC_SKIP_INSTALL ON)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
set(SHADERC_ENABLE_WERROR_COMPILE ON)
FetchContent_MakeAvailable(shaderc)

FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG "release-2.24.x"
        GIT_PROGRESS TRUE
        )
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
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
    )
set(SPDLOG_BUILD_EXAMPLE OFF)
set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(SPDLOG)

FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG v0.4.1
    GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable(rapidyaml)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.10.4
    GIT_PROGRESS TRUE
    )
set(JSON_BuildTests OFF)
FetchContent_MakeAvailable(json)

FetchContent_Declare(glm
        GIT_REPOSITORY "https://github.com/g-truc/glm.git"
        GIT_TAG "0.9.9.8"
        GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable(glm)

ExternalProject_Add(vma
    PREFIX "ThirdParty/vma"
    GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    GIT_TAG "0aa3989b8f382f185fdf646cc83a1d16fa31d6ab"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
    "<SOURCE_DIR>/include" "${CMAKE_CURRENT_BINARY_DIR}/include/vma"
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/vma"
    )
list(APPEND DEPEND_NAMES vma)

FetchContent_Declare(ktx_software
    GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
    GIT_TAG "adcccf152861158373311930e9b5e237d19168db"
    GIT_PROGRESS TRUE
    )
set(BASISU_SUPPORT_SSE "${BASISU_SUPPORT_SSE}")
set(KTX_FEATURE_TESTS OFF)
FetchContent_MakeAvailable(ktx_software)

add_subdirectory(Source)
