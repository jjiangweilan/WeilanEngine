#pragma once
#include "Engine/Driver/GfxDriver/CompiledSpv.hpp"
#include "Engine/Driver/GfxDriver/ShaderConfig.hpp"
#include "Engine/Driver/GfxDriver/ShaderPipelineInfo.hpp"
#include "ShaderLibraryAsyncWorker.hpp"
#include <filesystem>
#include <optional>
#include <string>

struct CompiledShaderData
{
    std::string shaderName;
    ShaderPermutation permutation;
    ShaderFeatures features;
    Gfx::ShaderPipelineInfo pipelineInfo;
    Gfx::PipelineConfig pipelineConfig;
    std::vector<uint8_t> vertexSpv;
    std::vector<uint8_t> fragmentSpv;
    std::vector<uint8_t> computeSpv;
};

class CompiledShaderLoader
{
public:
    static CompiledShaderLoader& Instance();

    // Try to load a compiled shader from cache
    std::optional<CompiledShaderData> LoadCompiledShader(
        const std::string& shaderName,
        ShaderPermutation permutation
    );

    // Load shader features without loading full shader data
    std::optional<ShaderFeatures> LoadShaderFeatures(const std::string& shaderName);

    // Check if a compiled shader exists
    bool HasCompiledShader(const std::string& shaderName, ShaderPermutation permutation);

    // Trigger shader recompilation (runs Python script)
    bool TriggerRecompilation();

    // Get the compiled shader root path
    static std::filesystem::path GetCompiledShaderRoot();

private:
    CompiledShaderLoader() = default;

    std::filesystem::path GetShaderDir(const std::string& shaderName);
    std::filesystem::path GetPermutationDir(const std::string& shaderName, ShaderPermutation permutation);
    std::string PermutationToString(ShaderPermutation permutation);

    std::optional<nlohmann::json> LoadShaderMeta(const std::string& shaderName);
    std::optional<nlohmann::json> LoadPermutationMeta(
        const std::string& shaderName,
        ShaderPermutation permutation
    );
    std::vector<uint8_t> LoadSpvFile(const std::filesystem::path& path);
};
