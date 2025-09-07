#pragma once
#include "Libs/DynamicArray.hpp"
#include "Libs/Hash.hpp"
#include "Rendering/Shader.hpp"
#include "Shader2.hpp"
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

#define MAX_SHADER_FEATURE_COUNT 64
using ShaderPermutation = std::bitset<MAX_SHADER_FEATURE_COUNT>;

struct ShaderToggleFeature
{
    std::string name = "";
    bool defaultValue = false;
};

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
    DeferredPBRShading,
    SceneLit,
    SceneLitSkinned,
    PlaneGrid,
    ImGui,
    LineShader,
    TriangleShader,
    JoltDebugShader,
    PostProcess_OutlineFullScreenPass,
    PostProcess_OutlineRawColorPass,
    PostProcess_SSAO,
    ShadowMapObject,
    ShadowMapObjectSkinned,
    ScreenSpaceShadow,
    FXAA,
    PrimitiveShape,
    SimpleForwardLit,
    SimpleColor,
    VolumetricCloud,
    Skybox,
    ColorGrading,
    InterleavedGradientNoise,
    SHProbe,
    Particle,
    Blit,
    BilateralUpScale,
    DepthDownSampler,
    ContactShadow,
    MAX_COUNT
};

class ShaderLibrary
{
public:
    static constexpr const char* ShaderNameMap[] = {
        "DeferredPBRShading",
        "SceneLit",
        "SceneLitSkinned",
        "PlaneGrid",
        "ImGui",
        "LineShader",
        "TriangleShader",
        "Specific/JoltDebugShader",
        "PostProcess/Outline/OutlineFullScreenPass",
        "PostProcess/Outline/OutlineRawColorPass",
        "PostProcess/SSAO",
        "ShadowMapObject",
        "ShadowMapObjectSkinned",
        "ScreenSpaceShadow",
        "FXAA",
        "PrimitiveShape",
        "SimpleForwardLit",
        "SimpleColor",
        "VolumetricCloud",
        "Skybox",
        "ColorGrading",
        "InterleavedGradientNoise",
        "SHProbe",
        "Particles/Particle",
        "Blit",
        "BilateralUpScale",
        "DepthDownSampler",
        "ContactShadow/ContactShadow"
    };

    // ***** Deprecating *****
    static constexpr const char* DeferredPBRShading = "DeferredPBRShading";
    static constexpr const char* SceneLit = "SceneLit";
    static constexpr const char* SceneLitSkinned = "SceneLitSkinned";
    static constexpr const char* PlaneGrid = "PlaneGrid";
    static constexpr const char* ImGui = "ImGui";
    static constexpr const char* LineShader = "LineShader";
    static constexpr const char* TriangleShader = "TriangleShader";
    static constexpr const char* JoltDebugShader = "Specific/JoltDebugShader";
    static constexpr const char* PostProcess_OutlineFullScreenPass = "PostProcess/Outline/OutlineFullScreenPass";
    static constexpr const char* PostProcess_OutlineRawColorPass = "PostProcess/Outline/OutlineRawColorPass";
    static constexpr const char* PostProcess_SSAO = "PostProcess/SSAO";
    static constexpr const char* ShadowMapObject = "ShadowMapObject";
    static constexpr const char* ShadowMapObjectSkinned = "ShadowMapObjectSkinned";
    static constexpr const char* ScreenSpaceShadow = "ScreenSpaceShadow";
    static constexpr const char* FXAA = "FXAA";
    static constexpr const char* PrimitiveShape = "PrimitiveShape";
    static constexpr const char* SimpleForwardLit = "SimpleForwardLit";
    static constexpr const char* SimpleColor = "SimpleColor";
    static constexpr const char* VolumetricCloud = "VolumetricCloud";
    static constexpr const char* Skybox = "Skybox";
    static constexpr const char* ColorGrading = "ColorGrading";
    static constexpr const char* InterleavedGradientNoise = "InterleavedGradientNoise";
    static constexpr const char* SHProbe = "SHProbe";
    static constexpr const char* Particle = "Particles/Particle";
    // **************
    //

    static const char* GetShaderName(Shaders shader) { return ShaderNameMap[(int)shader]; }

    static ObjPtr<Shader2> GetShader(Shaders shader, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(ShaderNameMap[(int)shader], permutation);
    }

    static ObjPtr<Shader2> GetShader(const char* name, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(name, permutation);
    }

    static ObjPtr<Shader2> GetShader(const char* name, const std::vector<std::string>& permutations)
    {
        return Singleton().GetShaderImpl(name, QueryShaderFeatures(name).GetPermutation(permutations));
    }

    static const ShaderFeatures& QueryShaderFeatures(const char* name)
    {
        return Singleton().QueryShaderFeaturesImpl(name);
    }

    static void DestorySlangInstance() { return Singleton().DestorySlangInstanceImpl(); }

    static void ReloadAllShaders() { return Singleton().ReloadAllShadersImpl(); }

    void RemoveAllShaders()
    {
        // calling .clear() may not actually clear the members
        library.clear();
    }

    void DestoryShaderLibrary();

    static ShaderLibrary& Singleton();

private:
    struct CompiledShader
    {
        CompiledShader() : shader(nullptr), permutation() {}
        CompiledShader(std::unique_ptr<Gfx::ShaderProgram>&& shader, ShaderPermutation permutation)
            : shader(std::move(shader)), shaderHandle(this->shader.get()), permutation(permutation)
        {}
        CompiledShader(CompiledShader&& other) = default;

        std::unique_ptr<Gfx::ShaderProgram> shader;
        Shader2 shaderHandle; // contains the shader object and return it to user
        ShaderPermutation permutation;

        void Recompile(ShaderLibrary* parent);
    };

    struct ShaderModule
    {
        ShaderFeatures features;
        std::unordered_map<ShaderPermutation, CompiledShader> shaders;
    };

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;
    std::unordered_map<std::string, ShaderModule> library;
    const char* shaderRootPath = GetShaderRootPath();
    std::mutex lk;

    ShaderLibrary();

    void Init();
    void LoadSession();
    void DestorySlangInstanceImpl();
    ObjPtr<Shader2> GetShaderImpl(const char* name, ShaderPermutation permutation = ShaderPermutation());
    const ShaderFeatures& QueryShaderFeaturesImpl(const char* name);
    void ReloadAllShadersImpl();
    inline const char* GetShaderRootPath() { return ENGINE_SOURCE_PATH "/Source/Engine/Shaders/"; }
    std::unique_ptr<Gfx::ShaderProgram> CompileShader(const char* shaderName, ShaderPermutation permutation);
    const ShaderFeatures& RetriveShaderFeatures(const char* shaderName);
    void CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& outFeatures);
    void CheckPushconstant(
        slang::VariableLayoutReflection* param,
        Slang::ComPtr<slang::IMetadata> entryPointMetaData[2],
        std::vector<Gfx::PipelineInfo::PushConstant>& outPushConstants,
        int entryPointIndex,
        Gfx::ShaderStage stage
    );
};
