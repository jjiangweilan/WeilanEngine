#pragma once
#include "Libs/DynamicArray.hpp"
#include "Libs/Hash.hpp"
#include "Rendering/Shader.hpp"
#include "Shader2.hpp"
#include "ShaderLibraryAsyncWorker.hpp"
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

#define SHADER_ENUMS(Do) Do(DeferredPBRShading, "DeferredPBRShading"), Do(SceneLit, "SceneLit"),             \
                         Do(SceneLitSkinned, "SceneLitSkinned"), Do(PlaneGrid, "PlaneGrid"),                 \
                         Do(ImGui, "ImGui"), Do(LineShader, "LineShader"),                                   \
                         Do(TriangleShader, "TriangleShader"),                                               \
                         Do(JoltDebugShader, "Specific/JoltDebugShader"),                                    \
                         Do(PostProcess_OutlineFullScreenPass, "PostProcess/Outline/OutlineFullScreenPass"), \
                         Do(PostProcess_OutlineRawColorPass, "PostProcess/Outline/OutlineRawColorPass"),     \
                         Do(PostProcess_SSAO, "PostProcess/SSAO"),                                           \
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
                         Do(Ocean, "Ocean"),

#define _SHADER_ENUMS_PICK_FIRST(x, y) x
#define _SHADER_ENUMS_PICK_SECOND(x, y) y

enum class Shaders : int
{
    SHADER_ENUMS(_SHADER_ENUMS_PICK_FIRST) MAX_COUNT
};

class ShaderLibrary
{
    struct CompiledShader
    {
        CompiledShader() : shader(nullptr), permutation() {}
        CompiledShader(std::unique_ptr<Gfx::ShaderProgram>&& shader, ShaderPermutation permutation)
            : shader(std::move(shader)), shaderHandle(this->shader.get()),
              permutation(permutation) {}
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
    ShaderLibraryAsyncWorker asyncWorker;

public:
    static constexpr const char* ShaderNameMap[] = {
        SHADER_ENUMS(_SHADER_ENUMS_PICK_SECOND)
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
    static constexpr const char* PostProcess_OutlineFullScreenPass =
        "PostProcess/Outline/OutlineFullScreenPass";
    static constexpr const char* PostProcess_OutlineRawColorPass =
        "PostProcess/Outline/OutlineRawColorPass";
    static constexpr const char* PostProcess_SSAO = "PostProcess/SSAO";
    static constexpr const char* ShadowMapObject = "ShadowMapObject";
    static constexpr const char* ShadowMapObjectSkinned =
        "ShadowMapObjectSkinned";
    static constexpr const char* ScreenSpaceShadow = "ScreenSpaceShadow";
    static constexpr const char* FXAA = "FXAA";
    static constexpr const char* PrimitiveShape = "PrimitiveShape";
    static constexpr const char* SimpleForwardLit = "SimpleForwardLit";
    static constexpr const char* SimpleColor = "SimpleColor";
    static constexpr const char* VolumetricCloud = "VolumetricCloud";
    static constexpr const char* Skybox = "Skybox";
    static constexpr const char* ColorGrading = "ColorGrading";
    static constexpr const char* InterleavedGradientNoise =
        "InterleavedGradientNoise";
    static constexpr const char* SHProbe = "SHProbe";
    static constexpr const char* Particle = "Particles/Particle";
    // **************
    //

    static const char* GetShaderName(Shaders shader)
    {
        return ShaderNameMap[(int)shader];
    }

    static ObjPtr<Shader2>
    GetShader(Shaders shader, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(ShaderNameMap[(int)shader], permutation);
    }

    static ObjPtr<Shader2>
    GetShader(const char* name, ShaderPermutation permutation = ShaderPermutation())
    {
        return Singleton().GetShaderImpl(name, permutation);
    }

    static void CompileAllDefaultShaders()
    {
        return Singleton().CompileAllDefaultShadersImpl();
    }

    static ObjPtr<Shader2>
    GetShader(const char* name, const std::vector<std::string>& permutations)
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

    static void DestorySlangInstance()
    {
        return Singleton().DestorySlangInstanceImpl();
    }

    static void ReloadAllShaders() { return Singleton().ReloadAllShadersImpl(); }

    void RemoveAllShaders()
    {
        // calling .clear() may not actually clear the members
        library.clear();
    }

    void DestoryShaderLibrary();

    static ShaderLibrary& Singleton();

private:
    ShaderLibrary();

    void Init();
    void LoadSession();
    void WaitForAllImpl() { asyncWorker.WaitForAll(); }
    void DestorySlangInstanceImpl();
    ObjPtr<Shader2>
    GetShaderImpl(const char* name, ShaderPermutation permutation = ShaderPermutation());
    const ShaderFeatures& QueryShaderFeaturesImpl(const char* name);
    void ReloadAllShadersImpl();
    inline const char* GetShaderRootPath()
    {
        return ENGINE_SOURCE_PATH "/Source/Engine/Shaders/";
    }
    std::unique_ptr<Gfx::ShaderProgram>
    CompileShader(const char* shaderName, ShaderPermutation permutation);
    const ShaderFeatures& RetriveShaderFeatures(const char* shaderName);
    void CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& outFeatures);
    void CompileAllDefaultShadersImpl();
    void CheckPushconstant(
        slang::VariableLayoutReflection* param,
        Slang::ComPtr<slang::IMetadata> entryPointMetaData[2],
        std::vector<Gfx::PipelineInfo::PushConstant>& outPushConstants,
        int entryPointIndex,
        Gfx::ShaderStage stage
    );
};
