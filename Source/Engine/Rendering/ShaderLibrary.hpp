#pragma once
#include "Libs/Hash.hpp"
#include "Rendering/Shader.hpp"
#include "Shader2.hpp"
#include <bitset>
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

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
        ShaderPermutation perm;
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

    std::vector<std::string> GetFeautresFromBitmask(ShaderPermutation permutation)
    {
        std::vector<std::string> result{};
        for (int bit = 0; bit < permutation.size(); ++bit)
        {
            if (permutation.test(bit))
            {
                result.push_back(bitMaskToFeature[bit]);
            }
        }

        return result;
    }

    std::unordered_map<uint32_t, std::string> bitMaskToFeature;
    std::unordered_map<std::string, uint32_t> featureToBitMask;
    std::vector<ShaderToggleFeature> toggleFeatures;
};

class ShaderLibrary
{
public:
    static constexpr const char* DeferredPBRShading = "DeferredPBRShading";
    static constexpr const char* SceneLit = "SceneLit";
    static constexpr const char* PlaneGrid = "PlaneGrid";
    static constexpr const char* ImGui = "ImGui";
    static constexpr const char* LineShader = "LineShader";
    static constexpr const char* TriangleShader = "TriangleShader";
    static constexpr const char* JoltDebugShader = "Specific/JoltDebugShader";
    static constexpr const char* PostProcess_OutlineFullScreenPass = "PostProcess/Outline/OutlineFullScreenPass";
    static constexpr const char* PostProcess_OutlineRawColorPass = "PostProcess/Outline/OutlineRawColorPass";
    static constexpr const char* PostProcess_SSAO = "PostProcess/SSAO";
    static constexpr const char* ShadowMapObject = "ShadowMapObject";
    static constexpr const char* ScreenSpaceShadow = "ScreenSpaceShadow";

    static ObjPtr<Shader2> GetShader(const char* name, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(name, permutation);
    }

    static const ShaderFeatures& QueryShaderFeatures(const char* name)
    {
        return Singleton().QueryShaderFeaturesImpl(name);
    }

    static ShaderLibrary& Singleton();
    void RemoveAllShaders() { library.clear(); }

private:
    ObjPtr<Shader2> GetShaderImpl(const char* name, ShaderPermutation permutation = ShaderPermutation());
    const ShaderFeatures& QueryShaderFeaturesImpl(const char* name);

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
    };

    struct ShaderModule
    {
        ShaderFeatures features;
        std::unordered_map<ShaderPermutation, CompiledShader> shaders;
    };

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;
    std::unordered_map<std::string, ShaderModule> library;

    ShaderLibrary();
    const char* shaderRootPath = GetShaderRootPath();
    inline const char* GetShaderRootPath() { return ENGINE_SOURCE_PATH "/Source/Engine/Shaders/"; }
    std::unique_ptr<Gfx::ShaderProgram> ComputeShader(const char* shaderName, ShaderPermutation permutation);
    const ShaderFeatures& RetriveShaderFeatures(const char* shaderName);

    void CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& outFeatures);
    void CollectVertexInputs(
        slang::EntryPointReflection* vertexEntryPointReflection,
        std::vector<Gfx::PipelineInfo::VertexAttribute>& outVertexAttributes,
        std::vector<Gfx::PipelineInfo::PushConstant>& outPushConstant
    );
    void CheckPushconstant(
        slang::VariableLayoutReflection* param,
        Slang::ComPtr<slang::IMetadata> entryPointMetaData[2],
        std::vector<Gfx::PipelineInfo::PushConstant>& outPushConstants,
        int entryPointIndex,
        Gfx::ShaderStage stage
    );
    // Gfx::DescriptorType MapDescriptorType();
};
