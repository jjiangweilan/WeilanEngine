enable_testing()
file(GLOB_RECURSE CORE_TEST_ENGINE_SRC "./*.c" "./*.cpp" "./*.hpp" "./*.h" "./*.tpp")
add_executable(TestEngine
    ${CORE_TEST_ENGINE_SRC}
)
if (INSTALL_DEPENDENCY)
    add_dependencies(TestEngine googletest)
endif()
target_include_directories(
    TestEngine

    SYSTEM
    PUBLIC
    "${CMAKE_BINARY_DIR}/gtest/include"
)
target_link_directories(TestEngine
    PUBLIC
    "${CMAKE_BINARY_DIR}/gtest/lib"
)
target_link_libraries(TestEngine
    WeilanEngineLib
    gtest
)
