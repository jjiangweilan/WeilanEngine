# ShaderCompilerTool - Standalone shader compiler
# This tool compiles Slang shaders and outputs SPIR-V binaries and metadata JSON.

add_executable(ShaderCompilerTool
    ShaderCompilerTool.cpp
)

target_include_directories(ShaderCompilerTool PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
)

target_link_libraries(ShaderCompilerTool PRIVATE
    slang
    ryml::ryml
    nlohmann_json::nlohmann_json
    Boost::program_options
    fmt
    spdlog
)

target_compile_definitions(ShaderCompilerTool PRIVATE
    SPDLOG_FMT_EXTERNAL
)

set_target_properties(ShaderCompilerTool PROPERTIES
    FOLDER "Tools"
)
