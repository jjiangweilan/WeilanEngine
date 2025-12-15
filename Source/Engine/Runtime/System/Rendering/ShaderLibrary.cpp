#include "ShaderLibrary.hpp"
#include "Driver/GfxDriver/GfxDriver.hpp"
#include "Library/Utils.hpp"
#include "Runtime/System/Rendering/EnumStringMapping.hpp"
#include <Library/Assert.hpp>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>

ShaderLibrary::ShaderLibrary()
{
}

ShaderLibrary& ShaderLibrary::Singleton()
{
    static ShaderLibrary singleton;
    return singleton;
}

Shader* ShaderLibrary::GetShaderImpl(const char* name, ShaderPermutation permutation)
{
    std::scoped_lock lk(syncAccess);

    auto shaderIter = library.find(name);
    if (shaderIter != library.end())
    {
        auto perm = shaderIter->second.shaders.find(permutation);
        if (perm != shaderIter->second.shaders.end())
        {
            return &perm->second.shaderHandle;
        }
    }

    asyncWorker.CompileShader(name, permutation);
    asyncWorker.WaitForAll();
    while (std::optional<AsyncCompiledData> compiled = asyncWorker.PollCompiled())
    {
        library[name].shaders.emplace(
            compiled->permutation,
            CompiledShader(std::move(compiled->shader), compiled->permutation)
        );
        library[name].features = compiled->shaderFeature;
    }

    return &library.at(name).shaders.at(permutation).shaderHandle;
}

const ShaderFeatures& ShaderLibrary::QueryShaderFeaturesImpl(const char* name)
{
    return asyncWorker.RetriveShaderFeatures(name);
}

void ShaderLibrary::ReloadAllShadersImpl()
{
    asyncWorker.ReloadAllShaders();
    asyncWorker.WaitForAll();

    // TODO: removed shader is not handled, they remains in this process session
    while (std::optional<AsyncCompiledData> compiled = asyncWorker.PollCompiled())
    {
        auto& compiledCache = library[compiled->name];
        auto& shaders = compiledCache.shaders;
        auto iter = shaders.find(compiled->permutation);
        if (iter != shaders.end())
        {
            iter->second.ReplaceShader(std::move(compiled->shader));
        }
        else
        {
            shaders.emplace(
                compiled->permutation,
                CompiledShader(std::move(compiled->shader), compiled->permutation)
            );
        }

        compiledCache.features = compiled->shaderFeature;
    }
}

void ShaderLibrary::CompiledShader::ReplaceShader(std::unique_ptr<Gfx::ShaderProgram>&& newShader)
{
    if (newShader)
    {
        shader = std::move(newShader);
        shaderHandle.ReplaceShader(shader.get());
    }
}

void ShaderLibrary::DestoryShaderLibrary()
{
    library.clear();
}

void ShaderLibrary::CompileAllDefaultShadersImpl()
{

    for (int i = 0; i < (int)Shaders::MAX_COUNT; ++i)
    {
        asyncWorker.CompileShader(ShaderLibrary::ShaderNameMap[i], 0);
    }

    asyncWorker.WaitForAll();

    while (std::optional<AsyncCompiledData> compiled = asyncWorker.PollCompiled())
    {
        library[compiled->name].shaders.emplace(
            compiled->permutation,
            CompiledShader(std::move(compiled->shader), compiled->permutation)
        );
        library[compiled->name].features = compiled->shaderFeature;
    }
}
