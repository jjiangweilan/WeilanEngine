cmake_minimum_required(VERSION 3.18)

# set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" )
project(WeilanEngine)

option(INSTALL_DEPENDENCY "install/update dependencies" OFF)
cmake_policy(SET CMP0114 NEW)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin" )
endif()

if (INSTALL_DEPENDENCY)
    include(ExternalProject)

    ExternalProject_Add(SPIRV-Cross
        PREFIX "ThirdParty/SPIRV-Cross"
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Cross.git"
        GIT_TAG "e9cc6403341baf0edd430a4027b074d0a06b782f"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/SPIRV-Cross"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DSPIRV_CROSS_CLI=OFF"
            "-DSPIRV_CROSS_ENABLE_TESTS=OFF"
    )
    list(APPEND DEPEND_NAMES SPIRV-Cross)
    
    ExternalProject_Add(SPDLOG
        PREFIX "ThirdParty/spdlog"
        GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
        GIT_TAG "v1.9.2"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spdlog"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DSPDLOG_BUILD_EXAMPLE=OFF"
    )
    list(APPEND DEPEND_NAMES SPDLOG)
    
    ExternalProject_Add(SDL2
        PREFIX "ThirdParty/SDL2"
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-2.0.18"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/SDL2"
        INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    )
    list(APPEND DEPEND_NAMES SDL2)

    ExternalProject_Add(rapidyaml
        PREFIX "ThirdParty/rapidyaml"
        GIT_REPOSITORY "https://github.com/biojppm/rapidyaml.git"
        GIT_TAG "v0.4.1"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/rapidyaml"
        INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    )
    list(APPEND DEPEND_NAMES rapidyaml)


     # Used by spirv-tools
     ExternalProject_Add(spirv-headers
         PREFIX "ThirdParty/spirv-headers"
         GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
         GIT_TAG "0bcc624926a25a2a273d07877fd25a6ff5ba1cfb" #from shaderc's DEPS file
         SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
         INSTALL_COMMAND ""
         CMAKE_ARGS 
             #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
             "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
             "-DSPIRV_HEADERS_SKIP_INSTALL=ON"
         #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
     )

     # Used by shaderc
     ExternalProject_Add(spirv-tools
         PREFIX "ThirdParty/spirv-tools"
         GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git"
         GIT_TAG "3a8a961cffb7699422a05dcbafdd721226b4547d" #from shaderc's DEPS file
         SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-tools"
         INSTALL_COMMAND ""
         DEPENDS spirv-headers
         CMAKE_ARGS 
             #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
             "-DSPIRV_SKIP_EXECUTABLES=ON"
             "-DCMAKE_CXX_STANDARD_LIBRARIES=${ClangMinGWLibs}" # spirv-tools doesn't handle stdlib linking when using clang-mingw, we add it here
             "-DSKIP_SPIRV_TOOLS_INSTALL=ON"
             "-DSPIRV-Headers_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
         #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
     )

    # Used by shaderc
    ExternalProject_Add(KHR_glslang
        PREFIX "ThirdParty/glslang"
        GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
        GIT_TAG "f771c1293dce29e1ac3557cf994169136155c81f" #from shaderc's DEPS file
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glslang"
        INSTALL_COMMAND ""
        # INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
        #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
           # "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DENABLE_CTEST=OFF"
            "-DENABLE_GLSLANG_BINARIES=OFF"
    )

    ExternalProject_Add(shaderc
        PREFIX "ThirdParty/shaderc"
        GIT_REPOSITORY "https://github.com/google/shaderc.git"
        GIT_TAG "v2022.2"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/shaderc"
        INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS spirv-tools KHR_glslang spirv-headers
        CMAKE_ARGS 
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        #"-DCMAKE_CXX_FLAGS=-Wno-deprecated-declarations"
        "-DSHADERC_SKIP_TESTS=ON"
        "-DSHADERC_SKIP_EXAMPLES=ON"
        "-DSHADERC_THIRD_PARTY_ROOT_DIR=${CMAKE_CURRENT_LIST_DIR}/ThirdParty"

        "-DCMAKE_CXX_STANDARD_LIBRARIES=${ClangMinGWLibs}"
        "-DENABLE_CTEST=OFF"
        "-DENABLE_GLSLANG_BINARIES=OFF"
        "-DSPIRV_SKIP_EXECUTABLES=ON"
        "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
        )
    list(APPEND DEPEND_NAMES shaderc)


    # ExternalProject_Add(ASSIMP
    #     PREFIX "ThirdParty/assimp"
    #     GIT_REPOSITORY "https://github.com/assimp/assimp.git"
    #     GIT_TAG "v5.2.3"
    #     SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/assimp"
    #     INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    #     CMAKE_ARGS 
    #         "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
    #         # it seems assimp can't use clang 12.0 right now, maybe we should use different compilers for different libraries
    #         "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    #         "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    #         "-DBUILD_SHARED_LIBS=ON"
    #         "-DASSIMP_BUILD_TESTS=OFF"
    #         # "-DASSIMP_BUILD_ZLIB=ON"
    #         "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF"
    #         "-DASSIMP_INSTALL_PDB=OFF"
    # )
    # list(APPEND DEPEND_NAMES ASSIMP)

    ExternalProject_Add(nlohmann-json
        PREFIX "ThirdParty/nlohmann-json"
        GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG "v3.10.4"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        "<SOURCE_DIR>/single_include" "${CMAKE_CURRENT_BINARY_DIR}/include"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/nlohmann-json"
        )
    list(APPEND DEPEND_NAMES nlohmann-json)

    ExternalProject_Add(glm
        PREFIX "ThirdParty/glm"
        GIT_REPOSITORY "https://github.com/g-truc/glm.git"
        GIT_TAG "0.9.9.8"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        "<SOURCE_DIR>/glm" "${CMAKE_CURRENT_BINARY_DIR}/include/glm"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glm"
        )
    list(APPEND DEPEND_NAMES glm)

    ExternalProject_Add(vma
        PREFIX "ThirdParty/vma"
        GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
        GIT_TAG "0c7ad4e85944b38ef6e4d2becfd58bf58385812e"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        "<SOURCE_DIR>/include" "${CMAKE_CURRENT_BINARY_DIR}/include/vma"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/vma"
        )
    list(APPEND DEPEND_NAMES vma)

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

add_subdirectory(Source)
