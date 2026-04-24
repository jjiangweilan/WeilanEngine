#include "ShaderLibrary.hpp"
#include "Engine/Driver/GfxDriver/ShaderPipelineInfoSerialization.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

CompiledShaderLoader& CompiledShaderLoader::Instance()
{
    static CompiledShaderLoader instance;
    return instance;
}

std::filesystem::path CompiledShaderLoader::GetCompiledShaderRoot()
{
#ifdef ENGINE_SOURCE_PATH
    return std::filesystem::path(ENGINE_SOURCE_PATH) / "Source" / "CompiledShader";
#else
    return std::filesystem::current_path() / "CompiledShader";
#endif
}

std::filesystem::path CompiledShaderLoader::GetShaderDir(const std::string& shaderName)
{
    return GetCompiledShaderRoot() / shaderName;
}

std::string CompiledShaderLoader::PermutationToString(ShaderPermutation permutation)
{
    return permutation.to_string();
}

std::filesystem::path CompiledShaderLoader::GetPermutationDir(
    const std::string& shaderName,
    ShaderPermutation permutation
)
{
    return GetShaderDir(shaderName) / "permutations" / PermutationToString(permutation);
}

std::optional<nlohmann::json> CompiledShaderLoader::LoadShaderMeta(const std::string& shaderName)
{
    auto metaPath = GetShaderDir(shaderName) / "shader_meta.json";
    if (!std::filesystem::exists(metaPath))
    {
        return std::nullopt;
    }

    try
    {
        std::ifstream file(metaPath);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        nlohmann::json meta;
        file >> meta;
        return meta;
    }
    catch (const std::exception& e)
    {
        spdlog::warn("Failed to load shader meta for {}: {}", shaderName, e.what());
        return std::nullopt;
    }
}

std::optional<nlohmann::json> CompiledShaderLoader::LoadPermutationMeta(
    const std::string& shaderName,
    ShaderPermutation permutation
)
{
    auto metaPath = GetPermutationDir(shaderName, permutation) / "permutation_meta.json";
    if (!std::filesystem::exists(metaPath))
    {
        return std::nullopt;
    }

    try
    {
        std::ifstream file(metaPath);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        nlohmann::json meta;
        file >> meta;
        return meta;
    }
    catch (const std::exception& e)
    {
        spdlog::warn("Failed to load permutation meta for {} [{}]: {}", shaderName, permutation.to_string(), e.what());
        return std::nullopt;
    }
}

std::vector<uint8_t> CompiledShaderLoader::LoadSpvFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        return {};
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}

bool CompiledShaderLoader::HasCompiledShader(const std::string& shaderName, ShaderPermutation permutation)
{
    auto permDir = GetPermutationDir(shaderName, permutation);
    auto metaPath = permDir / "permutation_meta.json";
    return std::filesystem::exists(metaPath);
}

std::optional<ShaderFeatures> CompiledShaderLoader::LoadShaderFeatures(const std::string& shaderName)
{
    auto shaderMeta = LoadShaderMeta(shaderName);
    if (!shaderMeta)
    {
        return std::nullopt;
    }

    try
    {
        ShaderFeatures features;

        // Load features array
        if (shaderMeta->contains("features"))
        {
            for (const auto& f : (*shaderMeta)["features"])
            {
                ShaderToggleFeature feature;
                feature.name = f.value("name", "");
                feature.defaultValue = f.value("defaultValue", false);
                features.toggleFeatures.push_back(feature);
            }
        }

        // Load feature to bit mask mapping
        if (shaderMeta->contains("featureToBitMask"))
        {
            for (auto& [key, value] : (*shaderMeta)["featureToBitMask"].items())
            {
                uint32_t bitIndex = value.get<uint32_t>();
                features.featureToBitMask[key] = bitIndex;
                features.bitMaskToFeature[bitIndex] = key;
            }
        }

        return features;
    }
    catch (const std::exception& e)
    {
        spdlog::warn("Failed to parse shader features for {}: {}", shaderName, e.what());
        return std::nullopt;
    }
}

std::optional<CompiledShaderData> CompiledShaderLoader::LoadCompiledShader(
    const std::string& shaderName,
    ShaderPermutation permutation
)
{
    // Load shader-level metadata
    auto shaderMeta = LoadShaderMeta(shaderName);
    if (!shaderMeta)
    {
        return std::nullopt;
    }

    // Load permutation-level metadata
    auto permMeta = LoadPermutationMeta(shaderName, permutation);
    if (!permMeta)
    {
        return std::nullopt;
    }

    try
    {
        CompiledShaderData data;
        data.shaderName = shaderName;
        data.permutation = permutation;

        // Load features from shader meta
        auto featuresOpt = LoadShaderFeatures(shaderName);
        if (featuresOpt)
        {
            data.features = std::move(*featuresOpt);
        }

        // Load pipeline config from shader meta
        if (shaderMeta->contains("pipelineConfig"))
        {
            data.pipelineConfig = Gfx::PipelineConfig::FromJson((*shaderMeta)["pipelineConfig"]);
        }

        // Load pipeline info from permutation meta
        if (permMeta->contains("pipelineInfo"))
        {
            data.pipelineInfo = (*permMeta)["pipelineInfo"].get<Gfx::ShaderPipelineInfo>();
        }

        // Load SPV files
        auto permDir = GetPermutationDir(shaderName, permutation);
        data.vertexSpv = LoadSpvFile(permDir / "vertex.spv");
        data.fragmentSpv = LoadSpvFile(permDir / "fragment.spv");
        data.computeSpv = LoadSpvFile(permDir / "compute.spv");

        // Verify we have at least some SPV data
        if (data.vertexSpv.empty() && data.fragmentSpv.empty() && data.computeSpv.empty())
        {
            spdlog::warn("No SPV data found for {} [{}]", shaderName, permutation.to_string());
            return std::nullopt;
        }

        return data;
    }
    catch (const std::exception& e)
    {
        spdlog::warn("Failed to load compiled shader {} [{}]: {}", shaderName, permutation.to_string(), e.what());
        throw;
        return std::nullopt;
    }
}

bool CompiledShaderLoader::TriggerRecompilation()
{
#ifdef ENGINE_SOURCE_PATH
    std::string scriptPath = std::string(ENGINE_SOURCE_PATH) + "/Source/Scripts/CompileShaders.py";
    std::string command = "python \"" + scriptPath + "\"";

    spdlog::info("Triggering shader recompilation...");

#ifdef _WIN32
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
            ENGINE_SOURCE_PATH,
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
            spdlog::info("Shader recompilation completed successfully");
            return true;
        }
        else
        {
            spdlog::error("Shader recompilation failed with exit code {}", exitCode);
            return false;
        }
    }
    else
    {
        spdlog::error("Failed to start shader compilation process");
        return false;
    }
#else
    int result = std::system(command.c_str());
    if (result == 0)
    {
        spdlog::info("Shader recompilation completed successfully");
        return true;
    }
    else
    {
        spdlog::error("Shader recompilation failed with exit code {}", result);
        return false;
    }
#endif

#else
    spdlog::error("Cannot trigger recompilation: ENGINE_SOURCE_PATH not defined");
    return false;
#endif
}
