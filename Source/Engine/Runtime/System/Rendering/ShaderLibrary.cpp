#include "ShaderLibrary.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"

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

    // Try to load from compiled cache first
    if (TryLoadFromCache(name, permutation))
    {
        return &library.at(name).shaders.at(permutation).shaderHandle;
    }

    return nullptr;
}

const ShaderFeatures& ShaderLibrary::QueryShaderFeaturesImpl(const char* name)
{
    std::scoped_lock lk(syncAccess);

    auto shaderIter = library.find(name);
    if (shaderIter != library.end())
    {
        return shaderIter->second.features;
    }

    // Try to load from compiled cache first
    if (TryLoadFromCache(name, ShaderPermutation())) // use default
    {
        return library.at(name).features;
    }

    static ShaderFeatures emptyFeatures{};
    return emptyFeatures;
}

void ShaderLibrary::ReloadAllShadersImpl()
{
    // asyncWorker.ReloadAllShaders();
    // asyncWorker.WaitForAll();

    // TODO: removed shader is not handled, they remains in this process session
    // while (std::optional<AsyncCompiledData> compiled = asyncWorker.PollCompiled())
    // {
    //     auto& compiledCache = library[compiled->name];
    //     auto& shaders = compiledCache.shaders;
    //     auto iter = shaders.find(compiled->permutation);
    //     if (iter != shaders.end())
    //     {
    //         iter->second.ReplaceShader(std::move(compiled->shader));
    //     }
    //     else
    //     {
    //         shaders.emplace(
    //             compiled->permutation,
    //             CompiledShader(std::move(compiled->shader), compiled->permutation)
    //         );
    //     }

    //     compiledCache.features = compiled->shaderFeature;
    // }
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
}

bool ShaderLibrary::TryLoadFromCache(const char* name, ShaderPermutation permutation)
{
    auto& loader = CompiledShaderLoader::Instance();

    auto compiledData = loader.LoadCompiledShader(name, permutation);
    if (!compiledData)
    {
        return false;
    }

    // Create shader program from cached SPV
    Gfx::PipelineCreateInfo createInfo{};
    createInfo.pipelineInfo = compiledData->pipelineInfo;
    createInfo.defaultConfig = compiledData->pipelineConfig;
    createInfo.vertSpv = std::move(compiledData->vertexSpv);
    createInfo.fragSpv = std::move(compiledData->fragmentSpv);
    createInfo.computeSpv = std::move(compiledData->computeSpv);

    auto shaderProgram = GetGfxDriver()->CreateShaderProgram(createInfo);
    if (!shaderProgram)
    {
        spdlog::warn("Failed to create shader program from cache for {}", name);
        return false;
    }

    // Store in library
    library[name].shaders.emplace(
        permutation,
        CompiledShader(std::move(shaderProgram), permutation)
    );
    library[name].features = compiledData->features;

    return true;
}

bool ShaderLibrary::TriggerShaderRecompilationImpl()
{
    return CompiledShaderLoader::Instance().TriggerRecompilation();
}
