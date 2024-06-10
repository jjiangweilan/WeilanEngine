enable_testing()

file(GLOB_RECURSE CORE_TEST_ENGINE_SRC "./*.c" "./*.cpp" "./*.hpp" "./*.h" "./*.tpp")

add_executable(EngineUnitTest
    ${CORE_TEST_ENGINE_SRC}
)

if (INSTALL_DEPENDENCY)
    add_dependencies(EngineUnitTest googletest)
endif()

target_include_directories(
    EngineUnitTest

    SYSTEM
    PUBLIC
    "${CMAKE_BINARY_DIR}/gtest/include"
)

target_link_directories(EngineUnitTest
    PUBLIC
    "${CMAKE_BINARY_DIR}/gtest/lib"
)

target_link_libraries(EngineUnitTest
    WeilanEngine
    gtest
)

set (TempFileDir "${CMAKE_BINARY_DIR}/tmp")
file(MAKE_DIRECTORY ${TempFileDir})

target_compile_definitions(EngineUnitTest
    PRIVATE
    TEMP_FILE_DIR="${TempFileDir}"
    )
