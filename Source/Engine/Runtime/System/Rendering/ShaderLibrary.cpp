#include "ShaderLibrary.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

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

void ShaderLibrary::ReloadAllShadersImpl(bool runCompilation)
{
    struct LoadedShaderKey
    {
        std::string name;
        ShaderPermutation permutation;
    };

    // Snapshot currently loaded shader permutations.
    // We must keep shader handles alive and only replace internal programs.
    std::vector<LoadedShaderKey> loadedShaders;
    {
        std::scoped_lock lk(syncAccess);
        for (auto& [name, cached] : library)
        {
            for (auto& [perm, _] : cached.shaders)
            {
                loadedShaders.push_back({name, perm});
            }
        }
    }

    if (runCompilation)
    {
        auto runCompileShadersTool = []() -> bool
        {
#ifdef ENGINE_SOURCE_PATH
            // Invoke CompileShaders.py directly (no dependency on cmake targets).
            const std::filesystem::path engineRoot = std::filesystem::path(ENGINE_SOURCE_PATH);
            const std::filesystem::path scriptPath = engineRoot / "Source" / "Scripts" / "CompileShaders.py";
            std::string command = "python \"" + scriptPath.string() + "\"";

#if defined(_WIN32)
            spdlog::info("Invoking local tool CompileShaders...");

            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi;

            if (CreateProcessA(
                    nullptr,
                    const_cast<char*>(command.c_str()),
                    nullptr,
                    nullptr,
                    FALSE,
                    0,
                    nullptr,
                    engineRoot.string().c_str(),
                    &si,
                    &pi
                ))
            {
                WaitForSingleObject(pi.hProcess, INFINITE);

                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);

                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode == 0)
                {
                    spdlog::info("CompileShaders completed successfully");
                    return true;
                }

                spdlog::error("CompileShaders failed with exit code {}", exitCode);
                return false;
            }

            spdlog::error("Failed to start CompileShaders process");
            return false;
#else
            spdlog::info("Invoking local tool CompileShaders...");
            int result = std::system(command.c_str());
            if (result == 0)
            {
                spdlog::info("CompileShaders completed successfully");
                return true;
            }
            spdlog::error("CompileShaders failed with exit code {}", result);
            return false;
#endif
#else
            spdlog::error("Cannot invoke CompileShaders: ENGINE_SOURCE_PATH not defined");
            return false;
#endif
        };

        if (!runCompileShadersTool())
        {
            // Keep existing in-memory shaders running.
            return;
        }
    }

    // Reload all shaders that are currently loaded in this process.
    auto& loader = CompiledShaderLoader::Instance();
    for (const auto& key : loadedShaders)
    {
        auto compiledData = loader.LoadCompiledShader(key.name, key.permutation);
        if (!compiledData)
        {
            spdlog::warn(
                "Failed to reload shader {} [{}] (missing compiled cache)",
                key.name,
                key.permutation.to_string()
            );
            continue;
        }

        Gfx::PipelineCreateInfo createInfo{};
        createInfo.pipelineInfo = compiledData->pipelineInfo;
        createInfo.defaultConfig = compiledData->pipelineConfig;
        createInfo.vertSpv = std::move(compiledData->vertexSpv);
        createInfo.fragSpv = std::move(compiledData->fragmentSpv);
        createInfo.computeSpv = std::move(compiledData->computeSpv);

        auto shaderProgram = GetGfxDriver()->CreateShaderProgram(createInfo);
        if (!shaderProgram)
        {
            spdlog::warn("Failed to create shader program while reloading {}", key.name);
            continue;
        }

        std::scoped_lock lk(syncAccess);
        auto& cached = library[key.name];
        cached.features = compiledData->features;

        auto iter = cached.shaders.find(key.permutation);
        if (iter != cached.shaders.end())
        {
            iter->second.ReplaceShader(std::move(shaderProgram));
        }
        else
        {
            cached.shaders.emplace(
                key.permutation,
                CompiledShader(std::move(shaderProgram), key.permutation)
            );
        }
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
