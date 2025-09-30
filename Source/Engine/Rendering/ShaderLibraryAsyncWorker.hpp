#pragma once
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "Shader2.hpp"
#include <memory>
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>

struct AsyncCompiledData
{
    std::string name;
    ShaderPermutation permutation;
    std::unique_ptr<Gfx::ShaderProgram> shader;
};

class ShaderLibraryAsyncWorker
{
public:
    ShaderLibraryAsyncWorker();
    void CompileShader(const char* name, ShaderPermutation permutation);

    std::optional<AsyncCompiledData> PollCompiled();

private:
    class CompileWorker;

    std::unique_ptr<CompileWorker> compileWorker;
};
