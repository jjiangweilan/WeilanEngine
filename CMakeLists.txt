set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0114 NEW)
option(INSTALL_DEPENDENCY "install/update dependencies" OFF)

set(DEPENDENCY_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if ("${CMAKE_OSX_SYSROOT}" STREQUAL "iphoneos")
    set(is_ios True)
    set(ios_platform "-iphoneos")
else()
    set(is_ios False)
    set(ios_platform "")
endif()

project(WeilanEngine)
if (INSTALL_DEPENDENCY)
    include(ExternalProject)

    if (APPLE)
        if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
            set (PHYSX_CONFIG_TYPE "$<IF:$<CONFIG:Release>,release,checked>")
            if (${is_ios})
                set (PHYSX_CONFIG_COMMAND sh ./generate_projects.sh ios64)
                set (PHYSX_BUILD_COMMAND cmake --build ./compiler/ios64 --target PhysX --config "${PHYSX_CONFIG_TYPE}")
                set (PHYSX_LIB_PATH "./bin/UNKNOWN/${PHYSX_CONFIG_TYPE}") # why UNKNOW? check here: https://github.com/o3de/PhysX/pull/2
            else()
                set (PHYSX_CONFIG_COMMAND sh ./generate_projects.sh mac-arm64)
                set (PHYSX_BUILD_COMMAND cmake --build ./compiler/mac-arm64 --target PhysX --config "${PHYSX_CONFIG_TYPE}")
                set (PHYSX_LIB_PATH "./bin/mac.x86_64/${PHYSX_CONFIG_TYPE}")
            endif()
            set (PHYSX_INSTALL_COMMAND 
                cmake -E copy_directory "./include/" "<INSTALL_DIR>/include/physx/" &&
                cmake -E copy_directory "${PHYSX_LIB_PATH}" "<INSTALL_DIR>/lib/")
            set (PHYSX_LIBS PhysX_static_64 PhysXCommon_static_64 PhysXFoundation_static_64)
        endif()
    endif(APPLE)

    if (WIN32)
        set (PHYSX_CONFIG_COMMAND ./generate_projects.bat vc17win64)
        set (PHYSX_CONFIG_TYPE "$<IF:$<CONFIG:Release>,release,checked>")
        set (PHYSX_BUILD_COMMAND cmake --build ./compiler/vc17win64 --target PhysX --config "${PHYSX_CONFIG_TYPE}")
        set (PHYSX_LIB_PATH "./bin/win.x86_64.vc142.mt/${PHYSX_CONFIG_TYPE}") # this is actually vc143, but it seems the naming can't be changed from our side: check this https://github.com/NVIDIA-Omniverse/PhysX/issues/59 (guess it's the same issue)
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
        GIT_REPOSITORY "https://github.com/jjiangweilan/PhysX.git"
        GIT_TAG "e23b94c3b4dbea4265493055eb61ca24cf99a663"
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
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
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
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
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
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        )
    list(APPEND DEPEND_NAMES SPDLOG)

    # if (WIN32)
    #     set (SDL2_INSTALL_COMMAND
    #         cmake --install "<BINARY_DIR>" &&
    #         cmake -E copy
    #     )
    # endif (WIND32)

    if (${is_ios})
        set(SDL2_BUILD_CMD BUILD_COMMAND xcodebuild -project <SOURCE_DIR>/Xcode/SDL/SDL.xcodeproj -target Framework-iOS -configuration "$CONFIGURATION")
        set(SDL2_INSTALL_CMD INSTALL_COMMAND cd .)
        set(SDL2_CONFIG_CMD CONFIGURE_COMMAND cd .)
    endif()
    ExternalProject_Add(SDL2
        PREFIX "ThirdParty/SDL2"
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-2.26.x"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/SDL2"
        INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        ${SDL2_CONFIG_CMD}
        ${SDL2_BUILD_CMD}
        ${SDL2_INSTALL_CMD}
        CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
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
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        )
    list(APPEND DEPEND_NAMES rapidyaml)


    # Used by spirv-tools
    ExternalProject_Add(spirv-headers
        PREFIX "ThirdParty/spirv-headers"
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
        GIT_TAG "4183b260f4cccae52a89efdfcdd43c4897989f42" #from shaderc's DEPS file
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
        INSTALL_COMMAND ""
        CMAKE_ARGS 
        #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON"
        "-DSPIRV_HEADERS_SKIP_INSTALL=ON"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )

    # Used by shaderc
    ExternalProject_Add(spirv-tools
        PREFIX "ThirdParty/spirv-tools"
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git"
        GIT_TAG "360d469b9eac54d6c6e20f609f9ec35e3a5380ad" #from shaderc's DEPS file
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-tools"
        INSTALL_COMMAND ""
        DEPENDS spirv-headers
        CMAKE_ARGS 
        #"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DSPIRV_SKIP_EXECUTABLES=ON"
        "-DSKIP_SPIRV_TOOLS_INSTALL=ON"
        "-DSPIRV-Headers_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}/ThirdParty/spirv-headers"
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )

    # Used by shaderc
    ExternalProject_Add(KHR_glslang
        PREFIX "ThirdParty/glslang"
        GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
        GIT_TAG "48f9ed8b08be974f4e463ef38136c8f23513b2cf" #from shaderc's DEPS file
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glslang"
        INSTALL_COMMAND ""
        # INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS 
        #"-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        # "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DENABLE_CTEST=OFF"
        "-DENABLE_GLSLANG_BINARIES=OFF"
        "-DALLOW_EXTERNAL_SPIRV_TOOLS=ON"
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        )

    ExternalProject_Add(shaderc_project
        PREFIX "ThirdParty/shaderc"
        GIT_REPOSITORY "https://github.com/jjiangweilan/shaderc.git"
        GIT_TAG "6ea8e20f13514a67af63de7da0b5a10dcb83a8b1"
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
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        )
    list(APPEND DEPEND_NAMES shaderc_project)

    ExternalProject_Add(ASSIMP
        PREFIX "ThirdParty/assimp"
        GIT_REPOSITORY "https://github.com/assimp/assimp.git"
        GIT_TAG "v5.3.1"
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
        "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
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

    if (${is_ios})
        set (KTX_FEATURE_TOOLS OFF)
    else()
        set (KTX_FEATURE_TOOLS ON)
    endif()
    ExternalProject_Add(ktx_software
        PREFIX "ThirdParty/ktx"
        GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
        GIT_TAG "adcccf152861158373311930e9b5e237d19168db"
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/ktx_software"
        INSTALL_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
        CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        "-DBASISU_SUPPORT_SSE=${BASISU_SUPPORT_SSE}"
        "-DKTX_FEATURE_TESTS=OFF"
        "-DKTX_FEATURE_TOOLS=${KTX_FEATURE_TOOLS}"
        "-DXCODE_CODE_SIGN_IDENTITY=${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}"
        "-DXCODE_DEVELOPMENT_TEAM=${CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
        "-DXCODE_PROVISIONING_PROFILE=Automatic"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
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
