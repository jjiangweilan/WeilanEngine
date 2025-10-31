#include "ShaderLibrary.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Utils.hpp"
#include "Rendering/EnumStringMapping.hpp"
#include <Libs/Assert.hpp>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
typedef SlangResult Result;
using Slang::ComPtr;

ShaderLibrary::ShaderLibrary()
{
    Init();
}

void ShaderLibrary::CheckPushconstant(
    slang::VariableLayoutReflection* param,
    Slang::ComPtr<slang::IMetadata> entryPointMetaData[2],
    std::vector<Gfx::ShaderPipelineInfo::PushConstant>& outPushConstants,
    int entryPointIndex,
    Gfx::ShaderStage stage
)
{
    int spaceIndex = param->getBindingSpace();
    int bindingIndex = param->getBindingIndex();
    if (bindingIndex >= outPushConstants.size())
    {
        outPushConstants.resize(bindingIndex + 1);
    }

    Gfx::ShaderPipelineInfo::PushConstant& pushConstant = outPushConstants[bindingIndex];
    bool used = false;
    entryPointMetaData[entryPointIndex]->isParameterLocationUsed(
        static_cast<SlangParameterCategory>(param->getCategory()),
        spaceIndex,
        bindingIndex,
        used
    );
    auto typeLayout = param->getTypeLayout();
    if (used)
    {
        pushConstant.size = typeLayout->getElementVarLayout()->getTypeLayout()->getSize();
        pushConstant.stages |= stage;
    }
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

void ShaderLibrary::CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& features)
{
    auto moduleReflection = module->getModuleReflection();
    for (auto child : moduleReflection->getChildren())
    {
        auto type = child->getKind();
        Slang::ComPtr<slang::IBlob> blob;
        if (type == slang::DeclReflection::Kind::Variable)
        {
            auto asVariable = child->asVariable();
            bool hasExtern = asVariable->findModifier(slang::Modifier::Extern) != nullptr;
            bool hasStatic = asVariable->findModifier(slang::Modifier::Static) != nullptr;
            bool hasConst = asVariable->findModifier(slang::Modifier::Const) != nullptr;

            if (hasExtern && hasStatic && hasConst)
            {
                ShaderToggleFeature f;
                f.name = child->getName();

                // TODO we need a way to know how to get default value
                f.defaultValue = false;
                features.push_back(f);
            }
        }
    }
}

const ShaderFeatures& ShaderLibrary::QueryShaderFeaturesImpl(const char* name)
{
    auto iter = library.find(name);
    if (iter == library.end())
    {
        return RetriveShaderFeatures(name);
    }

    return iter->second.features;
}

const ShaderFeatures& ShaderLibrary::RetriveShaderFeatures(const char* shaderName)
{
    globalSession = session->getGlobalSession();
    ComPtr<slang::IBlob> diagnostics;

    ComPtr<slang::IModule> module;
    module = session->loadModule(shaderName, diagnostics.writeRef());
    if (diagnostics)
    {
        spdlog::error("{}", (const char*)diagnostics->getBufferPointer());
    }
    ShaderFeatures features{};

    if (module)
    {
        CollectToggleFeatures(module, features.toggleFeatures);
        for (int featureIndex = 0; featureIndex < features.toggleFeatures.size() && featureIndex < 64; ++featureIndex)
        {
            features.featureToBitMask[features.toggleFeatures[featureIndex].name] = featureIndex;
            features.bitMaskToFeature[featureIndex] = features.toggleFeatures[featureIndex].name;
        }
    }

    library[shaderName].features = features;
    return library[shaderName].features; // returning a reference
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

void ShaderLibrary::CompiledShader::Recompile(ShaderLibrary* parent)
{
    //try
    //{
    //    auto newShader = parent->CompileShader(shader->GetName().c_str(), permutation);
    //    if (newShader)
    //    {
    //        shader = std::move(newShader);
    //        shaderHandle.ReplaceShader(shader.get());
    //    }
    //}
    //catch (std::exception e)
    //{
    //    spdlog::error(e.what());
    //}
}

void ShaderLibrary::DestoryShaderLibrary()
{
    session = nullptr;
    globalSession = nullptr;
    library.clear();
}

void ShaderLibrary::LoadSession()
{
    session = nullptr;

    slang::TargetDesc targetDesc{
        .structureSize = sizeof(slang::TargetDesc),
        .format = SlangCompileTarget::SLANG_SPIRV,
        .profile = globalSession->findProfile("spirv_1_6+spv_image_gather_extended"),
        .flags = 0
    };
    const char* searchPaths[] = {shaderRootPath};
    slang::PreprocessorMacroDesc preprocessorMacros[] = {{"CONFIG", "0"}, {"GPU_RESOURCE", "1"}};
    bool debug = true;

    slang::CompilerOptionValue debugLevel{};
    debugLevel.kind = slang::CompilerOptionValueKind::Int;
    debugLevel.intValue0 = debug ? SLANG_DEBUG_INFO_LEVEL_MAXIMAL : SLANG_DEBUG_INFO_LEVEL_NONE;

    slang::CompilerOptionValue debugFormat{};
    debugFormat.kind = slang::CompilerOptionValueKind::Int;
    debugFormat.intValue0 = SlangDebugInfoFormat::SLANG_DEBUG_INFO_FORMAT_DEFAULT;

    // https://github.com/KhronosGroup/SPIRV-Tools/issues/5959 we need this being fixed, til then we can safely debug an
    // optimized shader
    slang::CompilerOptionValue optimization{};
    optimization.kind = slang::CompilerOptionValueKind::Int;
    optimization.intValue0 = debug ? SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_NONE
                                   : SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_MAXIMAL;

    slang::CompilerOptionEntry compileOptions[] = {
        {slang::CompilerOptionName::DebugInformation, debugLevel},
        {slang::CompilerOptionName::DebugInformationFormat, debugFormat},
        {slang::CompilerOptionName::Optimization, optimization}
    };
    slang::SessionDesc sessionDesc{
        /** The size of this structure, in bytes.
         */
        .structureSize = sizeof(slang::SessionDesc),

        /** Code generation targets to include in the session.
         */
        .targets = &targetDesc,
        .targetCount = 1,

        /** Flags to configure the session.
         */
        .flags = slang::kSessionFlags_None,

        /** Default layout to assume for variables with matrix types.
         */
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,

        /** Paths to use when searching for `#include`d or `import`ed files.
         */
        .searchPaths = searchPaths,
        .searchPathCount = sizeof(searchPaths) / sizeof(const char*),

        .preprocessorMacros = preprocessorMacros,
        .preprocessorMacroCount = sizeof(preprocessorMacros) / sizeof(slang::PreprocessorMacroDesc),

        .fileSystem = nullptr,

        .enableEffectAnnotations = false,
        .allowGLSLSyntax = false,

        /** Pointer to an array of compiler option entries, whose size is compilerOptionEntryCount.
         */
        .compilerOptionEntries = compileOptions,

        /** Number of additional compiler option entries.
         */
        .compilerOptionEntryCount = sizeof(compileOptions) / sizeof(slang::CompilerOptionEntry),
    };

    globalSession->createSession(sessionDesc, session.writeRef());
}

void ShaderLibrary::Init()
{
    globalSession = nullptr;
    createGlobalSession(globalSession.writeRef());

    LoadSession();
}

void ShaderLibrary::DestorySlangInstanceImpl()
{
    session = nullptr;
    globalSession = nullptr;
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
