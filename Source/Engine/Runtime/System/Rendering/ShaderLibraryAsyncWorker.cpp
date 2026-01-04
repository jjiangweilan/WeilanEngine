#include "ShaderLibraryAsyncWorker.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>

typedef SlangResult Result;
using Slang::ComPtr;

struct CompileJobParams
{
    CompileJobParams() noexcept : shaderName(""), permutation() {}
    CompileJobParams(std::string&& name, ShaderPermutation&& permutation) noexcept
        : shaderName(std::move(name)), permutation(std::move(permutation))
    {}
    CompileJobParams(const std::string& name, const ShaderPermutation& permutation) noexcept
        : shaderName(name), permutation(permutation)
    {}

    // Add copy constructor and move operations as noexcept
    CompileJobParams(const CompileJobParams& other) noexcept
        : shaderName(other.shaderName), permutation(other.permutation)
    {}
    CompileJobParams(CompileJobParams&& other) noexcept
        : shaderName(std::move(other.shaderName)), permutation(std::move(other.permutation))
    {}
    CompileJobParams& operator=(const CompileJobParams& other) noexcept
    {
        if (this != &other)
        {
            shaderName = other.shaderName;
            permutation = other.permutation;
        }
        return *this;
    }
    CompileJobParams& operator=(CompileJobParams&& other) noexcept
    {
        if (this != &other)
        {
            shaderName = std::move(other.shaderName);
            permutation = std::move(other.permutation);
        }
        return *this;
    }

    ~CompileJobParams() noexcept = default;

    std::string shaderName;
    ShaderPermutation permutation;
};

class ShaderLibraryAsyncWorker::CompileWorker
{
    struct CachedShaderInformation
    {
        ShaderFeatures shaderFeatures;
    };

    struct CompiledShaderVariant
    {
        std::string shaderName;
        ShaderPermutation permutation;
    };

    JobHandle workingThread;

    MPMCQueue<CompileJobParams> workQueue;
    MPMCQueue<AsyncCompiledData> compiledQueue;

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;
    std::unordered_map<std::string, CachedShaderInformation> compiledShaderInformationCache;
    std::vector<CompiledShaderVariant> compiledShaderVariants;

    // mutex used to protect querying from other threads
    std::mutex queryMutex;

public:
    CompileWorker() : workQueue(32), compiledQueue(32) { Init(); }

    void Init()
    {
        globalSession = nullptr;
        createGlobalSession(globalSession.writeRef());

        LoadSession();
    }

    void ReloadAllShaders()
    {
        WaitForAll();
        GetGfxDriver()->WaitForIdle();

        LoadSession();

        auto copy = compiledShaderVariants;
        compiledShaderVariants.clear();

        for (auto& shader : copy)
        {
            PushWork(CompileJobParams{shader.shaderName, shader.permutation});
        }
    }

    void WaitForAll() { workingThread.Wait(); }

    void LoadSession()
    {
        session = nullptr;

        slang::TargetDesc targetDesc{
            .structureSize = sizeof(slang::TargetDesc),
            .format = SlangCompileTarget::SLANG_SPIRV,
            .profile = globalSession->findProfile("spirv_1_6+spv_image_gather_extended"),
            .flags = 0
        };
        const char* searchPaths[] = {GetShaderRootPath()};
        slang::PreprocessorMacroDesc preprocessorMacros[] = {{"CONFIG", "0"}, {"GPU_RESOURCE", "1"}};
        bool debug = true;

        slang::CompilerOptionValue debugLevel{};
        debugLevel.kind = slang::CompilerOptionValueKind::Int;
        debugLevel.intValue0 = debug ? SLANG_DEBUG_INFO_LEVEL_MAXIMAL : SLANG_DEBUG_INFO_LEVEL_NONE;

        slang::CompilerOptionValue debugFormat{};
        debugFormat.kind = slang::CompilerOptionValueKind::Int;
        debugFormat.intValue0 = SlangDebugInfoFormat::SLANG_DEBUG_INFO_FORMAT_DEFAULT;

        // https://github.com/KhronosGroup/SPIRV-Tools/issues/5959 we need this being fixed, til then we can safely
        // debug an optimized shader
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

    void PushWork(CompileJobParams params)
    {
        workQueue.try_push(std::move(params));
        if (!workingThread.IsValid() || workingThread.IsFinished())
        {
            TickOff();
        }
    }

    std::optional<AsyncCompiledData> PollCompiled()
    {
        AsyncCompiledData data;
        if (compiledQueue.try_pop(data))
        {
            return data;
        }

        return std::nullopt;
    }

    const ShaderFeatures& RetriveShaderFeatures(const char* shaderName)
    {
        std::scoped_lock alock(queryMutex);
        return RetriveShaderFeaturesNoLock(shaderName);
    }

private:
    inline const char* GetShaderRootPath() { return ENGINE_SOURCE_PATH "/Source/Engine/Shaders/"; }

    const ShaderFeatures& RetriveShaderFeaturesNoLock(const char* shaderName)
    {
        auto iter = compiledShaderInformationCache.find(shaderName);

        if (iter == compiledShaderInformationCache.end())
        {
            globalSession = session->getGlobalSession();
            ComPtr<slang::IBlob> diagnostics;

            ComPtr<slang::IModule> module;
            module = session->loadModule(shaderName, diagnostics.writeRef());
            ShaderCompiler::DiagnoseIfNeeded(diagnostics);

            ShaderFeatures features{};

            if (module)
            {
                CollectToggleFeatures(module, features.toggleFeatures);
                for (int featureIndex = 0; featureIndex < features.toggleFeatures.size() && featureIndex < 64;
                     ++featureIndex)
                {
                    features.featureToBitMask[features.toggleFeatures[featureIndex].name] = featureIndex;
                    features.bitMaskToFeature[featureIndex] = features.toggleFeatures[featureIndex].name;
                }
            }

            compiledShaderInformationCache[shaderName] = {features};
            return compiledShaderInformationCache[shaderName].shaderFeatures; // returning a reference
        }

        return iter->second.shaderFeatures;
    }

    void CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& features)
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

    void TickOff()
    {
        workingThread = JobSystem::Instance().Schedule(
            [this]()
            {
                // prevent race condition when querying shader features from other threads
                std::scoped_lock alock(queryMutex);

                while (!workQueue.empty())
                {
                    CompileJobParams params;
                    workQueue.pop(params);

                    ShaderFeatures compiledShaderFeature;
                    auto shaderProgram =
                        CompileShader(params.shaderName.data(), params.permutation, compiledShaderFeature);

                    compiledShaderVariants.push_back(
                        CompiledShaderVariant{params.shaderName, params.permutation}
                    );
                    AsyncCompiledData compiledData(
                        std::move(params.shaderName),
                        std::move(params.permutation),
                        std::move(compiledShaderFeature),
                        std::move(shaderProgram)
                    );
                    compiledQueue.try_push(std::move(compiledData));
                }
            }
        );
    }

    std::unique_ptr<Gfx::ShaderProgram> CompileShader(
        const char* shaderName, ShaderPermutation permutation, ShaderFeatures& outFeature
    )
    {
        ShaderCompiler compiler;
        Gfx::ShaderPipelineInfo pipelineInfo{};
        Gfx::PipelineConfig pipelineConfig{};
        auto& features = RetriveShaderFeaturesNoLock(shaderName);
        outFeature = features;
        auto featureStrings = features.GetFeautresFromBitmask(permutation);
        try
        {
            auto compileResult =
                compiler.CompileAndReflectProgram(session, shaderName, pipelineInfo, pipelineConfig, featureStrings);

            if (compileResult == SLANG_FAIL)
            {
                return nullptr;
            }
        }
        catch (std::exception e)
        {
            spdlog::critical("exception catched when compiling shader {}", e.what());
            return nullptr;
        }

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

            ShaderCompiler::DiagnoseIfNeeded(vertexDiagnostics);
        }

        if (compiler.fragmentEntryPointIndex != -1)
        {
            linkedProgram->getEntryPointCode(
                compiler.fragmentEntryPointIndex,
                0,
                fragmentKernelBlob.writeRef(),
                fragmentDiagnostics.writeRef()
            );

            ShaderCompiler::DiagnoseIfNeeded(fragmentDiagnostics);
        }

        if (compiler.computeEntryPointIndex != -1)
        {
            linkedProgram->getEntryPointCode(
                compiler.computeEntryPointIndex,
                0,
                computeKernelBlob.writeRef(),
                computeDiagnostics.writeRef()
            );

            ShaderCompiler::DiagnoseIfNeeded(computeDiagnostics);
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
};

ShaderLibraryAsyncWorker::ShaderLibraryAsyncWorker()
{
    compileWorker = std::make_unique<CompileWorker>();
}

ShaderLibraryAsyncWorker::~ShaderLibraryAsyncWorker() = default;

void ShaderLibraryAsyncWorker::CompileShader(const char* shaderName, ShaderPermutation permutation)
{
    compileWorker->PushWork(CompileJobParams(std::string(shaderName), permutation));
}

void ShaderLibraryAsyncWorker::WaitForAll()
{
    compileWorker->WaitForAll();
}

void ShaderLibraryAsyncWorker::ReloadAllShaders()
{
    compileWorker->ReloadAllShaders();
}

std::optional<AsyncCompiledData> ShaderLibraryAsyncWorker::PollCompiled()
{
    return compileWorker->PollCompiled();
}

void ShaderLibraryAsyncWorker::CleanUp()
{
    compileWorker = nullptr;
}

Gfx::ShaderPipelineInfo::Binding ShaderCompiler::AddBindingAsResource(const std::string& name, slang::TypeLayoutReflection* typeLayout, Gfx::ShaderPipelineInfo::DescriptorSet& set, uint32_t currentBinding)
{
    Gfx::ShaderPipelineInfo::Binding binding{};
    binding.name = name;
    binding.shaderBindingHandle = Gfx::ShaderBindingHandle(name);
    binding.bindingNum = currentBinding;
    binding.descriptorCount = 1; // TODO array binding
    binding.stages = MapSlangStageMask(slang::DescriptorTableSlot, set.setNum, currentBinding);
    binding.descriptorType = MapSlangDescriptorType(typeLayout, typeLayout->getResourceShape());
    if (binding.descriptorType == Gfx::DescriptorType::CombinedImageSampler ||
        binding.descriptorType == Gfx::DescriptorType::SampledImage ||
        binding.descriptorType == Gfx::DescriptorType::StorageImage)
    {
        binding.textureType = MapSlangTextureType(typeLayout->getResourceShape());
        binding.isTextureArray = IsTextureArray(typeLayout->getResourceShape());
    }
    else
        binding.textureType = Gfx::TextureType::Invalid;
    binding.bufferMembers = {};
    binding.byteSize = 0;
    binding.samplerIndex = AddSamplerConfig(set, typeLayout->getType(), binding.name);

    // TODO: currently slang can't report stage usage correctly
    // https://github.com/shader-slang/slang/issues/5940
    // if (binding.stages == Gfx::ShaderStage::None)
    {
        if (HasComputeEntryPoint())
        {
            binding.stages = Gfx::ShaderStage::Compute;
        }
        else
        {
            binding.stages = Gfx::ShaderStage::Fragment | Gfx::ShaderStage::Vertex;
        }
    }

    return binding;
}

const ShaderFeatures& ShaderLibraryAsyncWorker::RetriveShaderFeatures(const char* shaderName)
{
    return compileWorker->RetriveShaderFeatures(shaderName);
}
