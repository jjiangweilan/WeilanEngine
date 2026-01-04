/**
 * Standalone Shader Compiler Tool
 *
 * This tool compiles Slang shaders and outputs SPIR-V binaries and metadata JSON.
 * It uses the same compilation and reflection logic as the runtime ShaderLibraryAsyncWorker.
 *
 * Usage:
 *   ShaderCompilerTool --shader <shader_name> --output <output_dir> [--features feature1,feature2,...]
 *
 * Outputs to <output_dir>:
 *   - vertex.spv (if vertex shader exists)
 *   - fragment.spv (if fragment shader exists)
 *   - compute.spv (if compute shader exists)
 *   - metadata.json (pipeline info, config, features)
 */

#include "Engine/Driver/GfxDriver/ShaderPipelineInfo.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"
#include <boost/program_options.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <slang-com-ptr.h>
#include <slang.h>
#include <sstream>
#include <string>
#include <vector>

#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"

namespace po = boost::program_options;
namespace fs = std::filesystem;
using json = nlohmann::json;
using Slang::ComPtr;

// Forward declarations
struct ShaderToggleFeature
{
    std::string name;
    bool defaultValue = false;
};

static void DiagnoseIfNeeded(slang::IBlob* diagnostics)
{
    if (diagnostics)
    {
        std::cerr << (const char*)diagnostics->getBufferPointer() << std::endl;
    }
}

class ShaderCompilerTool
{
public:
    ComPtr<slang::IGlobalSession> globalSession;
    ComPtr<slang::ISession> session;

    std::vector<ComPtr<slang::IMetadata>> metadataForEntryPoints;
    int vertexEntryPointIndex = -1;
    int fragmentEntryPointIndex = -1;
    int computeEntryPointIndex = -1;
    ComPtr<slang::IComponentType> linkedProgram;
    slang::ProgramLayout* programLayout = nullptr;

    std::string shaderRoot;

    bool HasVertexEntryPoint() const { return vertexEntryPointIndex != -1; }
    bool HasFragmentEntryPoint() const { return fragmentEntryPointIndex != -1; }
    bool HasComputeEntryPoint() const { return computeEntryPointIndex != -1; }

    bool Init(const std::string& shaderRootPath)
    {
        shaderRoot = shaderRootPath;
        createGlobalSession(globalSession.writeRef());

        slang::TargetDesc targetDesc{
            .structureSize = sizeof(slang::TargetDesc),
            .format = SlangCompileTarget::SLANG_SPIRV,
            .profile = globalSession->findProfile("spirv_1_6"),
            .flags = 0
        };

        const char* searchPaths[] = {shaderRoot.c_str()};
        slang::PreprocessorMacroDesc preprocessorMacros[] = {{"CONFIG", "0"}, {"GPU_RESOURCE", "1"}};

        slang::CompilerOptionValue optimization{};
        optimization.kind = slang::CompilerOptionValueKind::Int;
        optimization.intValue0 = SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_NONE;

        slang::CompilerOptionEntry compileOptions[] = {
            {slang::CompilerOptionName::Optimization, optimization}
        };

        slang::SessionDesc sessionDesc{
            .structureSize = sizeof(slang::SessionDesc),
            .targets = &targetDesc,
            .targetCount = 1,
            .flags = slang::kSessionFlags_None,
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .searchPaths = searchPaths,
            .searchPathCount = 1,
            .preprocessorMacros = preprocessorMacros,
            .preprocessorMacroCount = 2,
            .fileSystem = nullptr,
            .enableEffectAnnotations = false,
            .allowGLSLSyntax = false,
            .compilerOptionEntries = compileOptions,
            .compilerOptionEntryCount = 1,
        };

        globalSession->createSession(sessionDesc, session.writeRef());
        return session != nullptr;
    }

    slang::IModule* LoadFeatureModule(const std::string& shaderName, const std::string& featureName)
    {
        auto srcString = fmt::format("export static const bool {} = true;", featureName);
        std::string moduleName = fmt::format("{}-{}", shaderName, featureName);
        std::string syntheticModulePath = fmt::format("_syntheticPath/{}.slang", moduleName);
        ComPtr<slang::IBlob> diagnostics;
        slang::IModule* toggleModule = session->loadModuleFromSourceString(
            moduleName.c_str(),
            syntheticModulePath.c_str(),
            srcString.c_str(),
            diagnostics.writeRef()
        );
        DiagnoseIfNeeded(diagnostics);
        return toggleModule;
    }

    void CollectToggleFeatures(slang::IModule* module, std::vector<ShaderToggleFeature>& features)
    {
        auto moduleReflection = module->getModuleReflection();
        for (auto child : moduleReflection->getChildren())
        {
            if (child->getKind() == slang::DeclReflection::Kind::Variable)
            {
                auto asVariable = child->asVariable();
                bool hasExtern = asVariable->findModifier(slang::Modifier::Extern) != nullptr;
                bool hasStatic = asVariable->findModifier(slang::Modifier::Static) != nullptr;
                bool hasConst = asVariable->findModifier(slang::Modifier::Const) != nullptr;

                if (hasExtern && hasStatic && hasConst)
                {
                    ShaderToggleFeature f;
                    f.name = child->getName();
                    f.defaultValue = false;
                    features.push_back(f);
                }
            }
        }
    }

    SlangResult CollectEntryPointMetadata(slang::IComponentType* program, int targetIndex, int entryPointCount)
    {
        metadataForEntryPoints.resize(entryPointCount);
        for (int entryPointIndex = 0; entryPointIndex < entryPointCount; entryPointIndex++)
        {
            ComPtr<slang::IMetadata> entryPointMetadata;
            ComPtr<slang::IBlob> diagnostics;
            SlangResult result = program->getEntryPointMetadata(
                entryPointIndex,
                targetIndex,
                entryPointMetadata.writeRef(),
                diagnostics.writeRef()
            );
            DiagnoseIfNeeded(diagnostics);
            SLANG_RETURN_ON_FAIL(result);
            metadataForEntryPoints[entryPointIndex] = entryPointMetadata;
        }
        return SLANG_OK;
    }

    bool CompileShader(
        const std::string& shaderName,
        const std::vector<std::string>& enabledFeatures,
        json& outMetadata,
        std::vector<uint8_t>& outVertexSpv,
        std::vector<uint8_t>& outFragmentSpv,
        std::vector<uint8_t>& outComputeSpv
    )
    {
        ComPtr<slang::IBlob> diagnostics;

        // Load module
        ComPtr<slang::IModule> sourceModule;
        sourceModule = session->loadModule(shaderName.c_str(), diagnostics.writeRef());
        DiagnoseIfNeeded(diagnostics);
        if (!sourceModule)
        {
            std::cerr << "Failed to load module: " << shaderName << std::endl;
            return false;
        }

        // Collect toggle features
        std::vector<ShaderToggleFeature> toggleFeatures;
        CollectToggleFeatures(sourceModule, toggleFeatures);

        std::vector<slang::IComponentType*> componentsToLink{};

        // Load feature modules
        for (const auto& enabledFeature : enabledFeatures)
        {
            auto featureModule = LoadFeatureModule(shaderName, enabledFeature);
            if (featureModule)
            {
                componentsToLink.push_back(featureModule);
            }
        }

        // Collect entry points
        std::string vertexShaderName, fragmentShaderName, computeShaderName;
        int definedEntryPointCount = sourceModule->getDefinedEntryPointCount();
        for (int i = 0; i < definedEntryPointCount; i++)
        {
            ComPtr<slang::IEntryPoint> entryPoint;
            sourceModule->getDefinedEntryPoint(i, entryPoint.writeRef());

            auto attribute = entryPoint->getFunctionReflection()->findAttributeByName(globalSession, "shader");
            if (attribute)
            {
                size_t size;
                const char* f = attribute->getArgumentValueString(0, &size);
                std::string stage(f, size);

                if (stage == "fragment")
                {
                    fragmentShaderName = entryPoint->getFunctionReflection()->getName();
                    fragmentEntryPointIndex = i;
                }
                else if (stage == "vertex")
                {
                    vertexShaderName = entryPoint->getFunctionReflection()->getName();
                    vertexEntryPointIndex = i;
                }
                else if (stage == "compute")
                {
                    computeShaderName = entryPoint->getFunctionReflection()->getName();
                    computeEntryPointIndex = i;
                }
            }
            componentsToLink.push_back(ComPtr<slang::IEntryPoint>(entryPoint));
        }

        // Compose and link
        ComPtr<slang::IComponentType> composed;
        auto result = session->createCompositeComponentType(
            componentsToLink.data(),
            componentsToLink.size(),
            composed.writeRef(),
            diagnostics.writeRef()
        );
        DiagnoseIfNeeded(diagnostics);
        if (SLANG_FAILED(result))
            return false;

        result = composed->link(linkedProgram.writeRef(), diagnostics.writeRef());
        DiagnoseIfNeeded(diagnostics);
        if (SLANG_FAILED(result))
            return false;

        programLayout = linkedProgram->getLayout(0, diagnostics.writeRef());
        DiagnoseIfNeeded(diagnostics);
        if (!programLayout)
            return false;

        SLANG_RETURN_FALSE_ON_FAIL(CollectEntryPointMetadata(linkedProgram, 0, definedEntryPointCount));

        // Build pipeline info
        json pipelineInfo;
        pipelineInfo["name"] = shaderName;
        pipelineInfo["vertexShaderName"] = vertexShaderName;
        pipelineInfo["fragmentShaderName"] = fragmentShaderName;
        pipelineInfo["computeShaderName"] = computeShaderName;
        pipelineInfo["isVertexInterleaved"] = false;
        pipelineInfo["vertexInputs"] = json::array();
        pipelineInfo["fragmentOutputs"] = json::array();
        pipelineInfo["descriptorSets"] = json::array();
        pipelineInfo["pushConstants"] = json::array();
        pipelineInfo["shaderDynamicStateFlags"] = 0;

        // Collect descriptor sets
        CollectSets(programLayout->getGlobalParamsVarLayout(), pipelineInfo);

        // Collect vertex inputs
        if (HasVertexEntryPoint())
        {
            CollectVertexInputs(pipelineInfo);
        }

        // Collect fragment outputs
        if (HasFragmentEntryPoint())
        {
            CollectFragmentOutputs(pipelineInfo);
        }

        // Get pipeline config from source file
        json pipelineConfig = GetPipelineConfig(sourceModule, pipelineInfo);

        // Get SPIR-V code
        if (HasVertexEntryPoint())
        {
            ComPtr<slang::IBlob> kernelBlob, kernelDiagnostics;
            linkedProgram->getEntryPointCode(vertexEntryPointIndex, 0, kernelBlob.writeRef(), kernelDiagnostics.writeRef());
            DiagnoseIfNeeded(kernelDiagnostics);
            if (kernelBlob)
            {
                outVertexSpv.resize(kernelBlob->getBufferSize());
                memcpy(outVertexSpv.data(), kernelBlob->getBufferPointer(), kernelBlob->getBufferSize());
            }
        }

        if (HasFragmentEntryPoint())
        {
            ComPtr<slang::IBlob> kernelBlob, kernelDiagnostics;
            linkedProgram->getEntryPointCode(fragmentEntryPointIndex, 0, kernelBlob.writeRef(), kernelDiagnostics.writeRef());
            DiagnoseIfNeeded(kernelDiagnostics);
            if (kernelBlob)
            {
                outFragmentSpv.resize(kernelBlob->getBufferSize());
                memcpy(outFragmentSpv.data(), kernelBlob->getBufferPointer(), kernelBlob->getBufferSize());
            }
        }

        if (HasComputeEntryPoint())
        {
            ComPtr<slang::IBlob> kernelBlob, kernelDiagnostics;
            linkedProgram->getEntryPointCode(computeEntryPointIndex, 0, kernelBlob.writeRef(), kernelDiagnostics.writeRef());
            DiagnoseIfNeeded(kernelDiagnostics);
            if (kernelBlob)
            {
                outComputeSpv.resize(kernelBlob->getBufferSize());
                memcpy(outComputeSpv.data(), kernelBlob->getBufferPointer(), kernelBlob->getBufferSize());
            }
        }

        // Build feature info
        json featuresJson = json::array();
        json featureToBitMask = json::object();
        for (size_t i = 0; i < toggleFeatures.size() && i < 64; i++)
        {
            json featureObj;
            featureObj["name"] = toggleFeatures[i].name;
            featureObj["defaultValue"] = toggleFeatures[i].defaultValue;
            featuresJson.push_back(featureObj);
            featureToBitMask[toggleFeatures[i].name] = i;
        }

        // Build output metadata
        outMetadata["pipelineInfo"] = pipelineInfo;
        outMetadata["pipelineConfig"] = pipelineConfig;
        outMetadata["features"] = featuresJson;
        outMetadata["featureToBitMask"] = featureToBitMask;
        outMetadata["enabledFeatures"] = enabledFeatures;

        return true;
    }

private:
    std::string MapDescriptorType(slang::BindingType rangeType)
    {
        switch (rangeType)
        {
            case slang::BindingType::CombinedTextureSampler: return "CombinedImageSampler";
            case slang::BindingType::Sampler: return "Sampler";
            case slang::BindingType::MutableTexture: return "StorageImage";
            case slang::BindingType::Texture: return "SampledImage";
            case slang::BindingType::TypedBuffer: return "UniformTexelBuffer";
            case slang::BindingType::MutableTypedBuffer: return "StorageTexelBuffer";
            case slang::BindingType::ConstantBuffer: return "UniformBuffer";
            case slang::BindingType::ParameterBlock: return "UniformBuffer";
            case slang::BindingType::RawBuffer: return "StorageBuffer";
            case slang::BindingType::MutableRawBuffer: return "StorageBuffer";
            default: return "Invalid";
        }
    }

    std::string MapTextureType(SlangResourceShape shape)
    {
        if ((shape & SlangResourceShape::SLANG_TEXTURE_2D) != 0)
            return "Tex2D";
        else if ((shape & SlangResourceShape::SLANG_TEXTURE_3D) != 0)
            return "Tex3D";
        else if ((shape & SlangResourceShape::SLANG_TEXTURE_CUBE) != 0)
            return "TexCube";
        return "Invalid";
    }

    bool IsTextureArray(SlangResourceShape shape)
    {
        return (shape & SlangResourceShape::SLANG_TEXTURE_ARRAY_FLAG) != 0;
    }

    std::string MapScalarType(slang::TypeReflection::ScalarType type)
    {
        switch (type)
        {
            case slang::TypeReflection::ScalarType::Float32:
            case slang::TypeReflection::ScalarType::Float16: return "Float";
            case slang::TypeReflection::ScalarType::Int32:
            case slang::TypeReflection::ScalarType::Int16:
            case slang::TypeReflection::ScalarType::Int8: return "Int";
            case slang::TypeReflection::ScalarType::UInt32:
            case slang::TypeReflection::ScalarType::UInt64: return "UInt";
            default: return "Float";
        }
    }

    json CollectBufferMembers(slang::VariableLayoutReflection* variableLayout)
    {
        json members = json::array();
        auto typeLayout = variableLayout->getTypeLayout();
        auto fieldCount = typeLayout->getFieldCount();

        for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
        {
            auto field = typeLayout->getFieldByIndex(fieldIndex);
            auto fieldTypeLayout = field->getTypeLayout();
            auto fieldKind = fieldTypeLayout->getKind();
            int size = fieldTypeLayout->getSize();

            if (size == 0)
                continue;

            json member;
            member["name"] = field->getName();
            member["offset"] = field->getOffset();
            member["byteSize"] = size;
            member["count"] = 0;
            member["columnCount"] = 1;
            member["rowCount"] = 1;
            member["attributes"] = json::array();

            switch (fieldKind)
            {
                case slang::TypeReflection::Kind::Struct:
                    member["type"] = "Structure";
                    break;
                case slang::TypeReflection::Kind::Array:
                    member["count"] = fieldTypeLayout->getElementCount();
                    member["type"] = MapScalarType(fieldTypeLayout->getElementTypeLayout()->getScalarType());
                    break;
                case slang::TypeReflection::Kind::Matrix:
                    member["columnCount"] = fieldTypeLayout->getColumnCount();
                    member["rowCount"] = fieldTypeLayout->getRowCount();
                    member["type"] = MapScalarType(fieldTypeLayout->getScalarType());
                    break;
                case slang::TypeReflection::Kind::Vector:
                    member["rowCount"] = fieldTypeLayout->getElementCount();
                    member["type"] = MapScalarType(fieldTypeLayout->getScalarType());
                    break;
                case slang::TypeReflection::Kind::Scalar:
                    member["type"] = MapScalarType(fieldTypeLayout->getScalarType());
                    break;
                default:
                    continue;
            }

            auto variable = field->getVariable();
            for (unsigned i = 0; i < variable->getUserAttributeCount(); ++i)
            {
                member["attributes"].push_back(variable->getUserAttributeByIndex(i)->getName());
            }

            members.push_back(member);
        }

        return members;
    }

    int AddSamplerConfig(json& set, slang::TypeReflection* typeLayout, const std::string& name)
    {
        json config;
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool pointFilter = lowerName.find("point") != std::string::npos;
        bool clampSampleToBorder = lowerName.find("border") != std::string::npos;
        bool clampSample = lowerName.find("clamp") != std::string::npos;

        std::string samplerTypeName = typeLayout->getName();
        config["enableCompare"] = (samplerTypeName == "SamplerComparisonState");
        config["anisotropic"] = false;

        if (clampSampleToBorder)
            config["addressModeU"] = config["addressModeV"] = config["addressModeW"] = "ClampToBorder";
        else if (clampSample)
            config["addressModeU"] = config["addressModeV"] = config["addressModeW"] = "ClampToEdge";
        else
            config["addressModeU"] = config["addressModeV"] = config["addressModeW"] = "Repeat";

        if (pointFilter)
        {
            config["minFilter"] = "Nearest";
            config["magFilter"] = "Nearest";
        }
        else
        {
            config["minFilter"] = "Linear";
            config["magFilter"] = "Linear";
        }

        json& samplerConfigs = set["samplerConfigs"];
        samplerConfigs.push_back(config);
        return (int)samplerConfigs.size() - 1;
    }

    json AddBindingAsResource(const std::string& name, slang::TypeLayoutReflection* typeLayout, json& set, uint32_t currentBinding)
    {
        json binding;
        binding["name"] = name;
        binding["bindingNum"] = currentBinding;
        binding["descriptorCount"] = 1;

        int stages = 0;
        if (HasComputeEntryPoint())
            stages = 0b100;
        else
            stages = 0b11;

        binding["stages"] = stages;

        slang::BindingType rangeType = typeLayout->getBindingRangeType(0);
        binding["descriptorType"] = MapDescriptorType(rangeType);

        binding["textureType"] = "Invalid";
        binding["isTextureArray"] = false;

        std::string descType = binding["descriptorType"];
        if (descType == "CombinedImageSampler" ||
            descType == "SampledImage" ||
            descType == "StorageImage")
        {
            binding["textureType"] = MapTextureType(typeLayout->getResourceShape());
            binding["isTextureArray"] = IsTextureArray(typeLayout->getResourceShape());
        }

        binding["bufferMembers"] = json::array();
        binding["byteSize"] = 0;
        binding["samplerIndex"] = AddSamplerConfig(set, typeLayout->getType(), name);

        return binding;
    }

    void CollectBindings(
        slang::VariableLayoutReflection* variableLayout,
        json& set,
        int parentBinding,
        json& outBindings
    )
    {
        auto typeLayout = variableLayout->getTypeLayout();
        auto kind = typeLayout->getKind();
        auto bindingOffset = variableLayout->getOffset(slang::ParameterCategory::DescriptorTableSlot);
        auto currentBinding = parentBinding + bindingOffset;

        switch (kind)
        {
            case slang::TypeReflection::Kind::SamplerState:
                {
                    json binding;
                    binding["name"] = variableLayout->getName();
                    binding["bindingNum"] = currentBinding;
                    binding["descriptorCount"] = 1;

                    if (HasComputeEntryPoint())
                        binding["stages"] = 0b100;
                    else
                        binding["stages"] = 0b11;

                    binding["descriptorType"] = MapDescriptorType(typeLayout->getBindingRangeType(0));
                    binding["textureType"] = MapTextureType(typeLayout->getResourceShape());
                    binding["isTextureArray"] = IsTextureArray(typeLayout->getResourceShape());
                    binding["bufferMembers"] = json::array();
                    binding["byteSize"] = 0;
                    binding["samplerIndex"] = AddSamplerConfig(set, variableLayout->getType(), binding["name"]);

                    outBindings.push_back(binding);
                    break;
                }
            case slang::TypeReflection::Kind::Resource:
                {
                    outBindings.push_back(AddBindingAsResource(variableLayout->getName(), variableLayout->getTypeLayout(), set, currentBinding));
                    break;
                }
            case slang::TypeReflection::Kind::Struct:
                {
                    auto fieldCount = typeLayout->getFieldCount();
                    for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                    {
                        CollectBindings(
                            typeLayout->getFieldByIndex(fieldIndex),
                            set,
                            parentBinding + bindingOffset,
                            outBindings
                        );
                    }
                    break;
                }
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ShaderStorageBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
                {
                    auto elementVarLayout = typeLayout->getElementVarLayout();
                    int size = elementVarLayout->getTypeLayout()->getStride();
                    if (size != 0)
                    {
                        json binding;
                        binding["name"] = variableLayout->getName();
                        binding["bindingNum"] = currentBinding;
                        binding["descriptorCount"] = 1;

                        if (HasComputeEntryPoint())
                            binding["stages"] = 0b100;
                        else
                            binding["stages"] = 0b11;

                        binding["descriptorType"] = MapDescriptorType(variableLayout->getTypeLayout()->getBindingRangeType(0));

                        binding["textureType"] = "Invalid";
                        binding["bufferMembers"] = CollectBufferMembers(elementVarLayout);
                        binding["byteSize"] = size;
                        binding["samplerIndex"] = -1;

                        outBindings.push_back(binding);
                    }
                    CollectBindings(elementVarLayout, set, parentBinding + bindingOffset, outBindings);
                    break;
                }
            case slang::TypeReflection::Kind::Array:
                {
                    if (variableLayout->getCategory() == slang::ParameterCategory::DescriptorTableSlot)
                    {
                        auto elementTypeLayout = variableLayout->getTypeLayout()->getElementTypeLayout();
                        auto binding = AddBindingAsResource(variableLayout->getName(), elementTypeLayout, set, currentBinding);
                        binding["descriptorCount"] = variableLayout->getTypeLayout()->getElementCount();
                        outBindings.push_back(binding);
                    }
                    break;
                }
            default:
                break;
        }
    }

    void AccessSet(slang::VariableLayoutReflection* setLayoutReflection, json& pipelineInfo)
    {
        json set;
        set["setNum"] = setLayoutReflection->getOffset(slang::SubElementRegisterSpace);
        set["name"] = setLayoutReflection->getName();
        set["semantics"] = "Global";

        // Check for attributes
        for (unsigned i = 0; i < setLayoutReflection->getVariable()->getUserAttributeCount(); ++i)
        {
            auto attribute = setLayoutReflection->getVariable()->getUserAttributeByIndex(i);
            std::string name = attribute->getName();
            if (name == "Global")
                set["semantics"] = "Global";
            else if (name == "Material")
                set["semantics"] = "Material";
            else if (name == "Object")
                set["semantics"] = "Object";
        }

        set["bindings"] = json::array();
        set["samplerConfigs"] = json::array();
        CollectBindings(setLayoutReflection, set, 0, set["bindings"]);

        pipelineInfo["descriptorSets"].push_back(set);
    }

    void ErrorReport(const char* message)
    {
        throw std::runtime_error(message);
    }

    Gfx::GfxFormat MapSlangFormat(std::string_view str)
    {
        if (str == "rgba8")
        {
            return Gfx::GfxFormat::R8G8B8A8_UNorm;
        }

        return Gfx::GfxFormat::Invalid;
    }

    void CollectSets(slang::VariableLayoutReflection* scopeVarLayout, json& pipelineInfo)
    {
        auto scopeTypeLayout = scopeVarLayout->getTypeLayout();
        auto kind = scopeTypeLayout->getKind();

        if (kind == slang::TypeReflection::Kind::Struct)
        {
            unsigned paramCount = scopeTypeLayout->getFieldCount();
            for (unsigned i = 0; i < paramCount; i++)
            {
                auto param = scopeTypeLayout->getFieldByIndex(i);
                auto paramKind = param->getTypeLayout()->getKind();

                switch (paramKind)
                {
                    case slang::TypeReflection::Kind::ParameterBlock:
                        AccessSet(param, pipelineInfo);
                        break;
                    case slang::TypeReflection::Kind::ConstantBuffer:
                        // Check for push constant
                        if (param->getCategory() == slang::ParameterCategory::PushConstantBuffer)
                        {
                            json pushConstant;
                            pushConstant["stages"] = 0b111; // https://github.com/shader-slang/slang/issues/5685
                            pushConstant["size"] = param->getTypeLayout()->getElementTypeLayout()->getSize();
                            pipelineInfo["pushConstants"].push_back(pushConstant);
                        }
                        break;
                    default:
                        ErrorReport("Not handled parameter kind in CollectSets");
                        break;
                }
            }
        }
    }

    void CollectVertexInputs(json& pipelineInfo)
    {
        auto vertexEntryPoint = programLayout->getEntryPointByIndex(vertexEntryPointIndex);
        for (unsigned paramIndex = 0; paramIndex < vertexEntryPoint->getParameterCount(); ++paramIndex)
        {
            auto param = vertexEntryPoint->getParameterByIndex(paramIndex);
            CollectVertexVaryingInput(param, 0, pipelineInfo["vertexInputs"]);
        }
    }

    void CollectVertexVaryingInput(slang::VariableLayoutReflection* variableLayout, int locationOffset, json& outVertexInputs)
    {
        auto category = variableLayout->getCategory();
        if (category != slang::ParameterCategory::VaryingInput)
            return;

        auto kind = variableLayout->getType()->getKind();

        if (kind == slang::TypeReflection::Kind::Struct)
        {
            auto typeLayout = variableLayout->getTypeLayout();
            auto fieldCount = typeLayout->getFieldCount();
            auto offset = variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT);
            for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
            {
                CollectVertexVaryingInput(typeLayout->getFieldByIndex(fieldIndex), locationOffset + offset, outVertexInputs);
            }
        }
        else if (kind == slang::TypeReflection::Kind::Vector)
        {
            json vertexAttribute;
            vertexAttribute["name"] = variableLayout->getName();
            vertexAttribute["location"] = locationOffset + variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT);

            bool used = false;
            metadataForEntryPoints[vertexEntryPointIndex]->isParameterLocationUsed(
                SLANG_PARAMETER_CATEGORY_VERTEX_INPUT,
                0,
                vertexAttribute["location"].get<int>(),
                used
            );
            if (!used)
                return;

            const char* semanticNameStr = variableLayout->getSemanticName();
            VertexAttributeSemantics semanticName;
            int semanticIndex = 0;
            if (semanticNameStr)
            {
                semanticName = MapVertexAttributeSemantics(semanticNameStr);
                semanticIndex = variableLayout->getSemanticIndex();
            }
            else // use default semantics
            {
                VertexAttributeSemantics defaultSemantics[6] = {
                    VertexAttributeSemantics::Position,
                    VertexAttributeSemantics::Normal,
                    VertexAttributeSemantics::Tangent,
                    VertexAttributeSemantics::Texcoord,
                    VertexAttributeSemantics::Color,
                    VertexAttributeSemantics::Bone
                };
                semanticName = defaultSemantics[outVertexInputs.size() < 6 ? outVertexInputs.size() : 0];
                semanticIndex = 0;
            }
            vertexAttribute["semanticName"] = Gfx::VertexAttributeSemanticsToString(semanticName);
            vertexAttribute["semanticIndex"] = semanticIndex;

            auto byteSizeAttribute = variableLayout->getVariable()->findAttributeByName(globalSession, "format");
            auto elementCount = variableLayout->getType()->getElementCount();
            Gfx::GfxFormat format = Gfx::GfxFormat::Invalid;

            if (byteSizeAttribute)
            {
                size_t strLen = 0;
                const char* str = byteSizeAttribute->getArgumentValueString(0, &strLen);
                std::string_view strView(str, strLen);
                format = MapSlangFormat(strView);
                if (format == Gfx::GfxFormat::Invalid)
                {
                    std::cerr << "Warning: provided shader UnderlyingFormat type is not supported, fall back to R32G32B32A32_SFloat" << std::endl;
                    format = Gfx::GfxFormat::R32G32B32A32_SFloat;
                }
            }
            else
            {
                if (elementCount == 1)
                    format = Gfx::GfxFormat::R32_SFloat;
                else if (elementCount == 2)
                    format = Gfx::GfxFormat::R32G32_SFloat;
                else if (elementCount == 3)
                    format = Gfx::GfxFormat::R32G32B32_SFloat;
                else if (elementCount == 4)
                    format = Gfx::GfxFormat::R32G32B32A32_SFloat;
            }

            vertexAttribute["format"] = Gfx::MapGfxFormatToString(format);
            vertexAttribute["size"] = Gfx::MapGfxFormatToByteSize(format);
            outVertexInputs.push_back(vertexAttribute);
        }
    }

    void CollectFragmentOutputs(json& pipelineInfo)
    {
        auto fragmentEntryPoint = programLayout->getEntryPointByIndex(fragmentEntryPointIndex);
        auto resultVarLayout = fragmentEntryPoint->getResultVarLayout();
        CollectFragmentOutput(resultVarLayout, 0, pipelineInfo["fragmentOutputs"]);
    }

    void CollectFragmentOutput(slang::VariableLayoutReflection* variableLayout, int locationOffset, json& outFragmentOutputs)
    {
        auto category = variableLayout->getCategory();
        if (category != slang::ParameterCategory::VaryingOutput)
            return;

        auto kind = variableLayout->getTypeLayout()->getKind();

        if (kind == slang::TypeReflection::Kind::Struct)
        {
            auto typeLayout = variableLayout->getTypeLayout();
            auto fieldCount = typeLayout->getFieldCount();
            auto offset = variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_OUTPUT);
            for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
            {
                CollectFragmentOutput(typeLayout->getFieldByIndex(fieldIndex), locationOffset + offset, outFragmentOutputs);
            }
        }
        else if (kind == slang::TypeReflection::Kind::Vector)
        {
            json fragmentOutput;
            fragmentOutput["location"] = locationOffset + variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_OUTPUT);

            auto elementCount = variableLayout->getType()->getElementCount();
            if (elementCount == 1)
                fragmentOutput["format"] = "R32_SFloat";
            else if (elementCount == 2)
                fragmentOutput["format"] = "R32G32_SFloat";
            else if (elementCount == 3)
                fragmentOutput["format"] = "R32G32B32_SFloat";
            else if (elementCount == 4)
                fragmentOutput["format"] = "R32G32B32A32_SFloat";

            outFragmentOutputs.push_back(fragmentOutput);
        }
    }

    std::stringstream GetYAML(std::stringstream& f)
    {
        const int MAX_LENGTH = 1024;
        char* line = new char[MAX_LENGTH];
        std::stringstream yamlConfig;
        std::regex begin("\\s*#(if|ifdef)\\s+CONFIG\\s*");
        std::regex end("\\s*#endif\\s*");
        f.seekg(0, std::ios::beg);
        while (f.getline(line, MAX_LENGTH))
        { // find #if|ifdef CONFIG
            if (std::regex_match(line, begin))
            {
                while (f.getline(line, MAX_LENGTH))
                {
                    if (!std::regex_match(line, end))
                    {
                        yamlConfig << line << '\n';
                    }
                    else
                        goto yamlEnd;
                }
            }
        }
    yamlEnd:
        return yamlConfig;
    }

    json MapPipelineConfig(ryml::Tree& tree, json& info)
    {
        json config;
        config["color"] = {
            {"blends", json::array()},
            {"blendConstants", {1.0, 1.0, 1.0, 1.0}}
        };
        config["depth"] = {
            {"boundTestEnable", false}
        };

        ryml::NodeRef root = tree.rootref();
        if (root.empty())
            return config;

        bool isVertexInterleaved = false;
        if (root.has_child("interleaved"))
            root["interleaved"] >> isVertexInterleaved;
        info["isVertexInterleaved"] = isVertexInterleaved;

        if (root.has_child("dynamicState"))
        {
            const auto& dynamicStateArray = root["dynamicState"];
            int finalVal = 0;
            if (dynamicStateArray.is_seq())
            {
                for (const ryml::ConstNodeRef& val : dynamicStateArray)
                {
                    std::string str;
                    val >> str;
                    finalVal |= (int)Gfx::StringToShaderDynamicState(str);
                }
            }
            info["shaderDynamicStateFlags"] = finalVal;
        }

        if (root.has_child("input"))
        {
            if (root["input"].is_map())
            {
                // Not used in config, just parsing
            }
        }

        if (root.has_child("mask"))
        {
            if (root["mask"].is_seq())
            {
                throw std::runtime_error("Multiple render target mask not implemented");
            }
            else
            {
                std::string val;
                root["mask"] >> val;
                json state;
                state["colorWriteMask"] = (int)Utils::MapColorMask(val);
                state["blendEnable"] = false;     // Default
                state["srcColorBlendFactor"] = 6; // SrcAlpha
                state["dstColorBlendFactor"] = 7; // OneMinusSrcAlpha
                state["colorBlendOp"] = 0;        // Add
                state["srcAlphaBlendFactor"] = 6;
                state["dstAlphaBlendFactor"] = 7;
                state["alphaBlendOp"] = 0;
                config["color"]["blends"].push_back(state);
            }
        }

        if (root.has_child("polygonMode"))
        {
            std::string val;
            root["polygonMode"] >> val;
            config["polygonMode"] = (int)Utils::MapPolygonMode(val);
        }

        if (root.has_child("topology"))
        {
            std::string val;
            root["topology"] >> val;
            config["topology"] = (int)Utils::MapTopology(val);
        }

        if (root.has_child("blend"))
        {
            if (root["blend"].is_seq())
            {
                int i = 0;
                for (const ryml::NodeRef& iter : root["blend"])
                {
                    if (iter.is_val())
                    {
                        if (i >= config["color"]["blends"].size())
                        {
                            json state;
                            state["blendEnable"] = false;
                            state["colorWriteMask"] = 15; // All
                            state["srcColorBlendFactor"] = 6;
                            state["dstColorBlendFactor"] = 7;
                            state["colorBlendOp"] = 0;
                            state["srcAlphaBlendFactor"] = 6;
                            state["dstAlphaBlendFactor"] = 7;
                            state["alphaBlendOp"] = 0;
                            config["color"]["blends"].push_back(state);
                        }

                        json& state = config["color"]["blends"][i];
                        std::string val;
                        iter >> val;

                        std::regex blendWithColorPattern("(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)");
                        std::regex blendPattern("(\\w+)\\s+(\\w+)");
                        std::smatch m;
                        if (std::regex_match(val, m, blendWithColorPattern))
                        {
                            state["blendEnable"] = true;
                            state["srcColorBlendFactor"] = (int)Utils::MapBlendFactor(m[1].str());
                            state["srcAlphaBlendFactor"] = (int)Utils::MapBlendFactor(m[2].str());
                            state["dstColorBlendFactor"] = (int)Utils::MapBlendFactor(m[3].str());
                            state["dstAlphaBlendFactor"] = (int)Utils::MapBlendFactor(m[4].str());
                        }
                        else if (std::regex_match(val, m, blendPattern))
                        {
                            state["blendEnable"] = true;
                            int src = (int)Utils::MapBlendFactor(m[1].str());
                            int dst = (int)Utils::MapBlendFactor(m[2].str());
                            state["srcAlphaBlendFactor"] = src;
                            state["dstAlphaBlendFactor"] = dst;
                            state["srcColorBlendFactor"] = src;
                            state["dstColorBlendFactor"] = dst;

                            if (src == 1 && dst == 0) // One, Zero
                                state["blendEnable"] = false;
                        }
                    }
                    i++;
                }
            }
        }

        if (root.has_child("blendOp"))
        {
            if (root["blendOp"].is_seq())
            {
                int i = 0;
                for (const ryml::NodeRef& iter : root["blendOp"])
                {
                    if (i >= config["color"]["blends"].size())
                        break;

                    std::regex alphaOnly("(\\w+)");
                    std::regex withColor("(\\w+)\\s+(\\w+)");

                    std::string val;
                    iter >> val;

                    std::smatch m;
                    if (std::regex_match(val, m, withColor))
                    {
                        config["color"]["blends"][i]["colorBlendOp"] = (int)Utils::MapBlendOp(m[1].str());
                        config["color"]["blends"][i]["alphaBlendOp"] = (int)Utils::MapBlendOp(m[2].str());
                    }
                    else if (std::regex_match(val, m, alphaOnly))
                    {
                        int op = (int)Utils::MapBlendOp(m[1].str());
                        config["color"]["blends"][i]["alphaBlendOp"] = op;
                        config["color"]["blends"][i]["colorBlendOp"] = op;
                    }
                    i++;
                }
            }
        }

        if (root.has_child("cull"))
        {
            std::string val;
            if (root["cull"].is_keyval())
            {
                root["cull"] >> val;
                config["cullMode"] = (int)Utils::MapCullMode(val);
            }
        }

        if (root.has_child("depth"))
        {
            auto depth = root["depth"];
            bool testEnable = true;
            bool writeEnable = true;
            if (depth.has_child("testEnable"))
                depth["testEnable"] >> testEnable;
            if (depth.has_child("writeEnable"))
                depth["writeEnable"] >> writeEnable;
            config["depth"]["testEnable"] = testEnable;
            config["depth"]["writeEnable"] = writeEnable;

            std::string compOp = "greaterOrEqual";
            if (depth.has_child("compOp"))
                depth["compOp"] >> compOp;
            config["depth"]["compOp"] = (int)Utils::MapCompareOp(compOp);

            bool boundTestEnable = false;
            if (depth.has_child("boundTestEnable"))
                depth["boundTestEnable"] >> boundTestEnable;
            config["depth"]["boundTestEnable"] = boundTestEnable;

            if (depth.has_child("minBounds"))
            {
                float val;
                depth["minBounds"] >> val;
                config["depth"]["minBounds"] = val;
            }
            if (depth.has_child("maxBounds"))
            {
                float val;
                depth["maxBounds"] >> val;
                config["depth"]["maxBounds"] = val;
            }
            if (depth.has_child("depthBias"))
            {
                float val;
                depth["depthBias"] >> val;
                config["depth"]["depthBias"] = val;
            }
            if (depth.has_child("depthSlopBias"))
            {
                float val;
                depth["depthSlopBias"] >> val;
                config["depth"]["depthSlopBias"] = val;
            }
        }

        if (root.has_child("stencil"))
        {
            auto stencil = root["stencil"];
            bool testEnable = false;
            if (stencil.has_child("testEnable"))
                stencil["testEnable"] >> testEnable;
            config["stencil"]["testEnable"] = testEnable;

            if (!stencil.has_child("front") && !stencil.has_child("back"))
            {
                std::string failOp = "keep", passOp = "keep", depthFailOp = "keep", compareOp = "never";
                if (stencil.has_child("failOp"))
                    stencil["failOp"] >> failOp;
                if (stencil.has_child("passOp"))
                    stencil["passOp"] >> passOp;
                if (stencil.has_child("depthFailOp"))
                    stencil["depthFailOp"] >> depthFailOp;
                if (stencil.has_child("compareOp"))
                    stencil["compareOp"] >> compareOp;

                json front, back;
                front["failOp"] = back["failOp"] = (int)Utils::MapStencilOp(failOp);
                front["passOp"] = back["passOp"] = (int)Utils::MapStencilOp(passOp);
                front["depthFailOp"] = back["depthFailOp"] = (int)Utils::MapStencilOp(depthFailOp);
                front["compareOp"] = back["compareOp"] = (int)Utils::MapCompareOp(compareOp);

                int compareMask = 0, writeMask = 0, reference = 0;
                if (stencil.has_child("compareMask"))
                    stencil["compareMask"] >> compareMask;
                if (stencil.has_child("writeMask"))
                    stencil["writeMask"] >> writeMask;
                if (stencil.has_child("reference"))
                    stencil["reference"] >> reference;

                front["compareMask"] = back["compareMask"] = compareMask;
                front["writeMask"] = back["writeMask"] = writeMask;
                front["reference"] = back["reference"] = reference;

                config["stencil"]["front"] = front;
                config["stencil"]["back"] = back;
            }
            else
            {
                if (stencil.has_child("front"))
                {
                    auto src = stencil["front"];
                    json dst;
                    std::string val;
                    if (src.has_child("failOp"))
                    {
                        src["failOp"] >> val;
                        dst["failOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["failOp"] = 0;
                    if (src.has_child("passOp"))
                    {
                        src["passOp"] >> val;
                        dst["passOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["passOp"] = 0;
                    if (src.has_child("depthFailOp"))
                    {
                        src["depthFailOp"] >> val;
                        dst["depthFailOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["depthFailOp"] = 0;
                    if (src.has_child("compareOp"))
                    {
                        src["compareOp"] >> val;
                        dst["compareOp"] = (int)Utils::MapCompareOp(val);
                    }
                    else
                        dst["compareOp"] = 7;

                    int iVal;
                    if (src.has_child("compareMask"))
                    {
                        src["compareMask"] >> iVal;
                        dst["compareMask"] = iVal;
                    }
                    else
                        dst["compareMask"] = 0;
                    if (src.has_child("writeMask"))
                    {
                        src["writeMask"] >> iVal;
                        dst["writeMask"] = iVal;
                    }
                    else
                        dst["writeMask"] = 0;
                    if (src.has_child("reference"))
                    {
                        src["reference"] >> iVal;
                        dst["reference"] = iVal;
                    }
                    else
                        dst["reference"] = 0;
                    config["stencil"]["front"] = dst;
                }
                if (stencil.has_child("back"))
                {
                    auto src = stencil["back"];
                    json dst;
                    std::string val;
                    if (src.has_child("failOp"))
                    {
                        src["failOp"] >> val;
                        dst["failOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["failOp"] = 0;
                    if (src.has_child("passOp"))
                    {
                        src["passOp"] >> val;
                        dst["passOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["passOp"] = 0;
                    if (src.has_child("depthFailOp"))
                    {
                        src["depthFailOp"] >> val;
                        dst["depthFailOp"] = (int)Utils::MapStencilOp(val);
                    }
                    else
                        dst["depthFailOp"] = 0;
                    if (src.has_child("compareOp"))
                    {
                        src["compareOp"] >> val;
                        dst["compareOp"] = (int)Utils::MapCompareOp(val);
                    }
                    else
                        dst["compareOp"] = 7;

                    int iVal;
                    if (src.has_child("compareMask"))
                    {
                        src["compareMask"] >> iVal;
                        dst["compareMask"] = iVal;
                    }
                    else
                        dst["compareMask"] = 0;
                    if (src.has_child("writeMask"))
                    {
                        src["writeMask"] >> iVal;
                        dst["writeMask"] = iVal;
                    }
                    else
                        dst["writeMask"] = 0;
                    if (src.has_child("reference"))
                    {
                        src["reference"] >> iVal;
                        dst["reference"] = iVal;
                    }
                    else
                        dst["reference"] = 0;
                    config["stencil"]["back"] = dst;
                }
            }
        }

        return config;
    }

    json GetPipelineConfig(slang::IModule* module, json& pipelineInfo)
    {
        // Try to parse YAML config from shader file
        auto filePath = module->getFilePath();
        std::ifstream file(filePath);
        if (file)
        {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::stringstream yamlConfig = GetYAML(buffer);
            std::string yamlStr = yamlConfig.str();
            if (!yamlStr.empty())
            {
                try
                {
                    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlStr));
                    return MapPipelineConfig(tree, pipelineInfo);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Error parsing YAML config: " << e.what() << std::endl;
                }
            }
        }

        // Return default config if no YAML found or error
        json config;
        config["cullMode"] = 2;    // Back
        config["topology"] = 0;    // TriangleList
        config["polygonMode"] = 0; // Fill
        config["depth"] = {
            {"writeEnable", true},
            {"testEnable", true},
            {"compOp", 6},
            {"boundTestEnable", false},
            {"minBounds", 0.0},
            {"maxBounds", 1.0}
        };
        config["stencil"] = {
            {"testEnable", false},
            {"front", {{"failOp", 0}, {"passOp", 0}, {"depthFailOp", 0}, {"compareOp", 7}, {"compareMask", 0}, {"writeMask", 0}, {"reference", 0}}},
            {"back", {{"failOp", 0}, {"passOp", 0}, {"depthFailOp", 0}, {"compareOp", 7}, {"compareMask", 0}, {"writeMask", 0}, {"reference", 0}}}
        };
        config["color"] = {
            {"blends", json::array()},
            {"blendConstants", {1.0, 1.0, 1.0, 1.0}}
        };
        return config;
    }
};

int main(int argc, char* argv[])
{
    po::options_description desc("Shader Compiler Tool Options");
    desc.add_options()("help,h", "Show help message")("shader,s", po::value<std::string>(), "Shader name (module name, not file path)")("output,o", po::value<std::string>(), "Output directory for compiled files")("shader-root,r", po::value<std::string>(), "Root directory for shader sources")("features,f", po::value<std::string>()->default_value(""), "Comma-separated list of enabled features");

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    if (vm.count("help") || !vm.count("shader") || !vm.count("output") || !vm.count("shader-root"))
    {
        std::cout << desc << std::endl;
        return vm.count("help") ? 0 : 1;
    }

    std::string shaderName = vm["shader"].as<std::string>();
    std::string outputDir = vm["output"].as<std::string>();
    std::string shaderRoot = vm["shader-root"].as<std::string>();
    std::string featuresStr = vm["features"].as<std::string>();

    // Parse features
    std::vector<std::string> enabledFeatures;
    if (!featuresStr.empty())
    {
        std::stringstream ss(featuresStr);
        std::string feature;
        while (std::getline(ss, feature, ','))
        {
            if (!feature.empty())
            {
                enabledFeatures.push_back(feature);
            }
        }
    }

    // Create output directory
    fs::create_directories(outputDir);

    // Initialize compiler
    ShaderCompilerTool compiler;
    if (!compiler.Init(shaderRoot))
    {
        std::cerr << "Failed to initialize shader compiler" << std::endl;
        return 1;
    }

    // Compile shader
    json metadata;
    std::vector<uint8_t> vertexSpv, fragmentSpv, computeSpv;

    if (!compiler.CompileShader(shaderName, enabledFeatures, metadata, vertexSpv, fragmentSpv, computeSpv))
    {
        std::cerr << "Failed to compile shader: " << shaderName << std::endl;
        return 1;
    }

    // Write outputs
    fs::path outPath(outputDir);

    // Write SPIR-V files
    if (!vertexSpv.empty())
    {
        std::ofstream vertFile(outPath / "vertex.spv", std::ios::binary);
        vertFile.write(reinterpret_cast<const char*>(vertexSpv.data()), vertexSpv.size());
    }

    if (!fragmentSpv.empty())
    {
        std::ofstream fragFile(outPath / "fragment.spv", std::ios::binary);
        fragFile.write(reinterpret_cast<const char*>(fragmentSpv.data()), fragmentSpv.size());
    }

    if (!computeSpv.empty())
    {
        std::ofstream compFile(outPath / "compute.spv", std::ios::binary);
        compFile.write(reinterpret_cast<const char*>(computeSpv.data()), computeSpv.size());
    }

    // Write metadata
    std::ofstream metaFile(outPath / "metadata.json");
    metaFile << metadata.dump(2);

    std::cout << "Successfully compiled: " << shaderName << std::endl;
    return 0;
}
