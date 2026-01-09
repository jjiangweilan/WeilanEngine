#pragma once
#include "Shader.hpp"
#include "ShaderFeatureToggle.hpp"
#include <bitset>
#include <filesystem>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

#define SHADER_ENUMS(Do) Do(DeferredPBRShading, "DeferredPBRShading"), Do(SceneLit, "SceneLit"),             \
                         Do(SceneLitSkinned, "SceneLitSkinned"), Do(PlaneGrid, "PlaneGrid"),                 \
                         Do(ImGui, "ImGui"), Do(LineShader, "LineShader"),                                   \
                         Do(TriangleShader, "TriangleShader"),                                               \
                         Do(JoltDebugShader, "Specific/JoltDebugShader"),                                    \
                         Do(PostProcess_OutlineFullScreenPass, "PostProcess/Outline/OutlineFullScreenPass"), \
                         Do(PostProcess_OutlineRawColorPass, "PostProcess/Outline/OutlineRawColorPass"),     \
                         Do(PostProcess_SSAO, "PostProcess/SSAO"),                                           \
                         Do(PostProcess_Bloom, "PostProcess/Bloom"),                                         \
                         Do(ShadowMapObject, "ShadowMapObject"),                                             \
                         Do(ShadowMapObjectSkinned, "ShadowMapObjectSkinned"),                               \
                         Do(ScreenSpaceShadow, "ScreenSpaceShadow"), Do(FXAA, "FXAA"),                       \
                         Do(PrimitiveShape, "PrimitiveShape"),                                               \
                         Do(SimpleForwardLit, "SimpleForwardLit"),                                           \
                         Do(SimpleColor, "SimpleColor"), Do(Skybox, "Skybox"),                               \
                         Do(ColorGrading, "ColorGrading"),                                                   \
                         Do(InterleavedGradientNoise, "InterleavedGradientNoise"),                           \
                         Do(Particle, "Particles/Particle"), Do(Blit, "Blit"),                               \
                         Do(BilateralUpScale, "BilateralUpScale"),                                           \
                         Do(DepthDownSampler, "DepthDownSampler"),                                           \
                         Do(ContactShadow, "ContactShadow/ContactShadow"),                                   \
                         Do(Ocean, "Ocean"),                                                                 \
                         Do(ReflectionProbeIBLGenerator, "ReflectionProbeIBLGenerator"),                     \
                         Do(FidelityFX_SPD, "ffx_spd"),                                                      \
                         Do(ReflectionProbeSkybox, "ReflectionProbeSkybox"),                                 \
                         Do(DepthBasedFog, "DepthBasedFog"),                                                 \
                         Do(OceanPatchShader, "OceanPatchShader"),

#define _SHADER_ENUMS_PICK_FIRST(x, y) x
#define _SHADER_ENUMS_PICK_SECOND(x, y) y

#define MAX_SHADER_FEATURE_COUNT 64
using ShaderPermutation = std::bitset<MAX_SHADER_FEATURE_COUNT>;
struct ShaderFeatures
{
    template <class Iterable>
    ShaderPermutation GetPermutation(const Iterable& names) const
    {
        ShaderPermutation perm{};
        for (const std::string& name : names)
        {
            uint32_t bitIndex = 0;
            auto iter = featureToBitMask.find(name);
            if (iter != featureToBitMask.end())
            {
                bitIndex = iter->second;
                perm.set(bitIndex, true);
            }
        }

        return perm;
    }

    std::vector<std::string> GetFeautresFromBitmask(ShaderPermutation permutation) const
    {
        std::vector<std::string> result{};
        for (int bit = 0; bit < permutation.size(); ++bit)
        {
            if (permutation.test(bit))
            {
                result.push_back(bitMaskToFeature.at(bit));
            }
        }

        return result;
    }

    std::unordered_map<uint32_t, std::string> bitMaskToFeature{};
    std::unordered_map<std::string, uint32_t> featureToBitMask{};
    std::vector<ShaderToggleFeature> toggleFeatures{};
};

enum class Shaders : int
{
    SHADER_ENUMS(_SHADER_ENUMS_PICK_FIRST)
        MAX_COUNT
};

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

class ShaderLibrary
{
    struct CompiledShader
    {
        CompiledShader()
            : shader(nullptr), permutation() {}
        CompiledShader(std::unique_ptr<Gfx::ShaderProgram>&& shader, ShaderPermutation permutation)
            : shader(std::move(shader)), shaderHandle(this->shader.get()),
              permutation(permutation) {}
        CompiledShader(CompiledShader&& other) = default;

        std::unique_ptr<Gfx::ShaderProgram> shader;
        Shader shaderHandle; // contains the shader object and return it to user
        ShaderPermutation permutation;

        void ReplaceShader(std::unique_ptr<Gfx::ShaderProgram>&& newShader);
    };

    struct ShaderCached
    {
        ShaderFeatures features;
        std::unordered_map<ShaderPermutation, CompiledShader> shaders;
    };

    std::unordered_map<std::string, ShaderCached> library;
    const char* shaderRootPath = GetShaderRootPath();

    // TODO: shader compilation can be trigger in multithreading when loading resources like Material, a proper method is needed to handle this case
    std::mutex syncAccess;

public:
    static constexpr const char* ShaderNameMap[] = {
        SHADER_ENUMS(_SHADER_ENUMS_PICK_SECOND)
    };

    static const char* GetShaderName(Shaders shader)
    {
        return ShaderNameMap[(int)shader];
    }

    static Shader* GetShader(Shaders shader, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(ShaderNameMap[(int)shader], permutation);
    }

    static Shader* GetShader(const char* name, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(name, permutation);
    }

    static void CompileAllDefaultShaders()
    {
        return Singleton().CompileAllDefaultShadersImpl();
    }

    static Shader* GetShader(const char* name, const std::vector<std::string>& permutations)
    {
        return Singleton().GetShaderImpl(
            name,
            QueryShaderFeatures(name).GetPermutation(permutations)
        );
    }

    static const ShaderFeatures& QueryShaderFeatures(const char* name)
    {
        return Singleton().QueryShaderFeaturesImpl(name);
    }

    static void WaitForShaderCompilation() { Singleton().WaitForAllImpl(); }

    static void ReloadAllShaders() { return Singleton().ReloadAllShadersImpl(); }

    // Trigger shader recompilation via Python script
    static bool TriggerShaderRecompilation() { return Singleton().TriggerShaderRecompilationImpl(); }

    void RemoveAllShaders()
    {
        // calling .clear() may not actually clear the members
        library.clear();
    }

    void DestoryShaderLibrary();

    static ShaderLibrary& Singleton();

private:
    ShaderLibrary();

    void LoadSession();
    void WaitForAllImpl() {}
    Shader* GetShaderImpl(const char* name, ShaderPermutation permutation = ShaderPermutation());
    const ShaderFeatures& QueryShaderFeaturesImpl(const char* name);
    void ReloadAllShadersImpl();
    inline const char* GetShaderRootPath()
    {
        return ENGINE_SOURCE_PATH "/Source/Engine/Shaders/";
    }
    const ShaderFeatures& RetriveShaderFeatures(const char* shaderName);
    void CompileSingleShader();
    void CompileAllDefaultShadersImpl();
    bool TriggerShaderRecompilationImpl();

    // Try to load shader from compiled cache, returns true if successful
    bool TryLoadFromCache(const char* name, ShaderPermutation permutation);
};
