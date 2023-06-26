cmake_minimum_required(VERSION 3.18)

project(WeilanEngine)

option(INSTALL_DEPENDENCY "install/update dependencies" OFF)
if (INSTALL_DEPENDENCY)
    include(ExternalProject)

    if (APPLE)
        if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
            set (PHYSX_CONFIG_COMMAND sh ./generate_projects.sh mac-arm64)
            set (PHYSX_CONFIG_TYPE "$<IF:$<CONFIG:Release>,release,checked>")
            set (PHYSX_BUILD_COMMAND cmake --build ./compiler/mac-arm64 --target PhysX --config "${PHYSX_CONFIG_TYPE}")
            set (PHYSX_LIB_PATH "./bin/mac.x86_64/${PHYSX_CONFIG_TYPE}")
            set (PHYSX_INSTALL_COMMAND 
                    cmake -E copy_directory "./include/" "<INSTALL_DIR>/include/physx/" &&
                    cmake -E copy_directory "${PHYSX_LIB_PATH}" "<INSTALL_DIR>/lib/")
            set (PHYSX_LIBS PhysX_static_64 PhysXCommon_static_64 PhysXFoundation_static_64)
        endif()
    endif(APPLE)

    if (WIN32)
        set (PHYSX_CONFIG_COMMAND ./generate_projects.bat vc16win64)
        set (PHYSX_CONFIG_TYPE "$<IF:$<CONFIG:Release>,release,checked>")
        set (PHYSX_BUILD_COMMAND cmake --build ./compiler/vc16win64 --target PhysX --config "${PHYSX_CONFIG_TYPE}")
        set (PHYSX_LIB_PATH "./bin/win.x86_64.vc142.mt/${PHYSX_CONFIG_TYPE}")
        set (PHYSX_INSTALL_COMMAND 
                cmake -E copy_directory "./include/" "<INSTALL_DIR>/include/physx/" &&
                cmake -E copy_directory "${PHYSX_LIB_PATH}" "<INSTALL_DIR>/lib/" &&
                cmake -E copy   "<INSTALL_DIR>/lib/PhysX_64.dll"
                                "<INSTALL_DIR>/lib/PhysXCommon_64.dll"
                                "<INSTALL_DIR>/lib/PhysXFoundation_64.dll"
                                "<INSTALL_DIR>/bin/")
        set (PHYSX_LIBS PhysX_64 PhysXCommon_64 PhysXFoundation_64)
    endif(WIN32)

    ExternalProject_Add(PhysX
        PREFIX "ThirdParty/PhysX"
        GIT_REPOSITORY "https://github.com/o3de/PhysX.git"
        GIT_TAG "60e7d82a3856740881d0c892a01df7fd4e0676f6"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/PhysX"
        BINARY_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/PhysX/physx"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CONFIGURE_COMMAND ${PHYSX_CONFIG_COMMAND}
        BUILD_COMMAND ${PHYSX_BUILD_COMMAND}
        INSTALL_COMMAND ${PHYSX_INSTALL_COMMAND}
    )
    list(APPEND DEPEND_NAMES PhysX)

    ExternalProject_Add(SPIRV-Cross
        PREFIX "ThirdParty/SPIRV-Cross"
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Cross.git"
        GIT_TAG "7cb2f62e3ba4d9acff27fea40ddc5a2844f39d5e"
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

    ExternalProject_Add(fmt_lib
        PREFIX "ThirdParty/fmt"
        GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
        GIT_TAG "9.1.0"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/fmt"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            #"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DFMT_DOC=OFF"
            "-DFMT_TEST=OFF"
            "-DFMT_SYSTEM_HEADERS=ON"
    )
    list(APPEND DEPEND_NAMES fmt_lib)
    
    ExternalProject_Add(SPDLOG
        PREFIX "ThirdParty/spdlog"
        GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
        GIT_TAG "v1.9.2"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spdlog"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS fmt_lib
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DSPDLOG_BUILD_EXAMPLE=OFF"
            "-DSPDLOG_FMT_EXTERNAL=ON"
    )
    list(APPEND DEPEND_NAMES SPDLOG)

    # if (WIN32)
    #     set (SDL2_INSTALL_COMMAND
    #         cmake --install "<BINARY_DIR>" &&
    #         cmake -E copy
    #     )
    # endif (WIND32)

    ExternalProject_Add(SDL2
        PREFIX "ThirdParty/SDL2"
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-2.24.x"
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
        "-DSHADERC_ENABLE_SHARED_CRT=ON"

        "-DENABLE_CTEST=OFF"
        "-DENABLE_GLSLANG_BINARIES=OFF"
        "-DSPIRV_SKIP_EXECUTABLES=ON"
        "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
        )
    list(APPEND DEPEND_NAMES shaderc)


    ExternalProject_Add(ASSIMP
        PREFIX "ThirdParty/assimp"
        GIT_REPOSITORY "https://github.com/assimp/assimp.git"
        GIT_TAG "v5.2.5"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/assimp"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            # "-DCMAKE_CXX_FLAGS=-Wno-deprecated-declarations"
            "-DBUILD_SHARED_LIBS=ON"
            "-DASSIMP_BUILD_TESTS=OFF"
            # "-DASSIMP_BUILD_ZLIB=ON"
            "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF"
            "-DASSIMP_INSTALL_PDB=OFF"
    )
    list(APPEND DEPEND_NAMES ASSIMP)

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
        GIT_TAG "0aa3989b8f382f185fdf646cc83a1d16fa31d6ab"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        "<SOURCE_DIR>/include" "${CMAKE_CURRENT_BINARY_DIR}/include/vma"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/vma"
        )
    list(APPEND DEPEND_NAMES vma)

    ExternalProject_Add(ktx_software
        PREFIX "ThirdParty/ktx"
        GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
        GIT_TAG "f47d1a2e73f93c9433298c5de0a616419b567a4b"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/ktx_software"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS
        "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        "-DBASISU_SUPPORT_SSE=${BASISU_SUPPORT_SSE}"
        "-DKTX_FEATURE_TESTS=OFF"
        )
    list(APPEND DEPEND_NAMES ktx_software)

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
