#include "ShaderLibrary.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ShaderLibrary_Internal.hpp"
#include <Libs/Assert.hpp>

ShaderLibrary::ShaderLibrary()
{
    Init();
}

std::unique_ptr<Gfx::ShaderProgram> ShaderLibrary::CompileShader(const char* shaderName, ShaderPermutation permutation)
{
    ShaderCompiler compiler;
    Gfx::PipelineInfo pipelineInfo{};
    Gfx::PipelineConfig pipelineConfig{};
    auto features = RetriveShaderFeatures(shaderName);
    auto featureStrings = features.GetFeautresFromBitmask(permutation);
    auto compileResult = compiler.CompileAndReflectProgram(session, shaderName, pipelineInfo, pipelineConfig, featureStrings);
    if (compileResult == SLANG_FAIL)
    {
        return nullptr;
    }

    {
        Gfx::PipelineCreateInfo createInfo{};
        createInfo.defaultConfig = pipelineConfig;
        createInfo.pipelineInfo = pipelineInfo;

        auto& linkedProgram = compiler.linkedProgram;
        Slang::ComPtr<slang::IBlob> vertexKernelBlob, vertexDiagnostics;
        Slang::ComPtr<slang::IBlob> fragmentKernelBlob, fragmentDiagnostics;
        Slang::ComPtr<slang::IBlob> computeKernelBlob, computeDiagnostics;
        if (compiler.vertexEntryPointIndex != -1)
        {
            linkedProgram->getEntryPointCode(
                compiler.vertexEntryPointIndex,
                0,
                vertexKernelBlob.writeRef(),
                vertexDiagnostics.writeRef()
            );
        }

        if (compiler.fragmentEntryPointIndex != -1)
        {
            linkedProgram->getEntryPointCode(
                compiler.fragmentEntryPointIndex,
                0,
                fragmentKernelBlob.writeRef(),
                fragmentDiagnostics.writeRef()
            );
        }

        if (compiler.computeEntryPointIndex != -1)
        {
            linkedProgram->getEntryPointCode(
                compiler.computeEntryPointIndex,
                0,
                computeKernelBlob.writeRef(),
                computeDiagnostics.writeRef()
            );
        }

        if (compiler.fragmentEntryPointIndex != -1 && compiler.vertexEntryPointIndex != -1)
        {
            createInfo.vertSpv = std::vector<uint8_t>(vertexKernelBlob->getBufferSize());
            createInfo.fragSpv = std::vector<uint8_t>(fragmentKernelBlob->getBufferSize());
            memcpy(createInfo.vertSpv.data(), vertexKernelBlob->getBufferPointer(), vertexKernelBlob->getBufferSize());
            memcpy(
                createInfo.fragSpv.data(),
                fragmentKernelBlob->getBufferPointer(),
                fragmentKernelBlob->getBufferSize()
            );
        }
        else if (compiler.computeEntryPointIndex != -1)
        {
            createInfo.computeSpv = std::vector<uint8_t>(computeKernelBlob->getBufferSize());
            memcpy(
                createInfo.computeSpv.data(),
                computeKernelBlob->getBufferPointer(),
                computeKernelBlob->getBufferSize()
            );
        }

        return GetGfxDriver()->CreateShaderProgram(createInfo);
    }

    // vertexInputs
    // {
    //     slang::EntryPointReflection* vertexEntryPointReflection =
    //         linkedProgram->getLayout()->getEntryPointByIndex(vertexEntryPointIndex);
    //     CollectVertexInputs(vertexEntryPointReflection, pipelineInfo.vertexInputs, pushConstants);
    // }

    //// bindings(descriptor sets)
    // auto globalParamsVarLayout = linkedProgram->getLayout()->getGlobalParamsVarLayout();
    // auto globalCat = globalParamsVarLayout->getCategory();
    // auto fieldCount = 1;
    // auto xx = globalParamsVarLayout->getTypeLayout()->getElementTypeLayout()->getParameterCategory();
    // for (int i = 0; i < fieldCount; ++i)
    //{
    //     auto ref = globalParamsVarLayout->getType()->getFieldByIndex(i);
    //     auto name = ref->getName();
    //     int x = 0;
    // }

    // createInfo.name = shaderName;

    // return GetGfxDriver()->CreateShaderProgram(createInfo);
};

void ShaderLibrary::CheckPushconstant(
    slang::VariableLayoutReflection* param,
    Slang::ComPtr<slang::IMetadata> entryPointMetaData[2],
    std::vector<Gfx::PipelineInfo::PushConstant>& outPushConstants,
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

    Gfx::PipelineInfo::PushConstant& pushConstant = outPushConstants[bindingIndex];
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

ObjPtr<Shader2> ShaderLibrary::GetShaderImpl(const char* name, ShaderPermutation permutation)
{
    std::scoped_lock lock(lk);
    auto shaderIter = library.find(name);
    if (shaderIter != library.end())
    {
        auto perm = shaderIter->second.shaders.find(permutation);
        if (perm != shaderIter->second.shaders.end())
        {
            return &perm->second.shaderHandle;
        }
    }

    std::unique_ptr<Gfx::ShaderProgram> newShader = CompileShader(name, permutation);
    if (newShader == nullptr)
    {
        return nullptr;
    }
    library[name].shaders.emplace(permutation, CompiledShader(std::move(newShader), permutation));
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
    return library[shaderName].features;
}

void ShaderLibrary::ReloadAllShadersImpl()
{
    GetGfxDriver()->WaitForIdle();
    Init();

    for (auto& shader : library)
    {
        for (auto& m : shader.second.shaders)
        {
            m.second.Recompile(this);
        }
    }
}

void ShaderLibrary::CompiledShader::Recompile(ShaderLibrary* parent)
{
    try
    {
        auto newShader = parent->CompileShader(shader->GetName().c_str(), permutation);
        if (newShader)
        {
            shader = std::move(newShader);
            shaderHandle.ReplaceShader(shader.get());
        }
    }
    catch (std::exception e)
    {
        spdlog::error(e.what());
    }
}
void ShaderLibrary::Init()
{
    globalSession = nullptr;
    session = nullptr;

    createGlobalSession(globalSession.writeRef());

    slang::TargetDesc targetDesc{
        .structureSize = sizeof(slang::TargetDesc),
        .format = SLANG_SPIRV,
        .profile = globalSession->findProfile("spirv_1_5"),
    };
    const char* searchPaths[] = {shaderRootPath};
    slang::PreprocessorMacroDesc preprocessorMacros[] = {{"CONFIG", "0"}, {"GPU_RESOURCE", "1"}};
    slang::CompilerOptionValue debugLevel{};
    bool debug = true;
    debugLevel.kind = slang::CompilerOptionValueKind::Int;
    debugLevel.intValue0 = debug ? SLANG_DEBUG_INFO_LEVEL_MAXIMAL : 0;
    slang::CompilerOptionEntry compileOptions[] = {{slang::CompilerOptionName::DebugInformation, debugLevel}};
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
        .compilerOptionEntryCount = 1,
    };

    globalSession->createSession(sessionDesc, session.writeRef());
}
