#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/VertexAttributes.hpp"
#include "Libs/Assert.hpp"
#include "Libs/Utils.hpp"
#include "Rendering/EnumStringMapping.hpp"
#include <condition_variable>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>
#include <vector>

typedef SlangResult Result;
using Slang::ComPtr;

struct ShaderCompiler
{
    std::vector<ComPtr<slang::IMetadata>> metadataForEntryPoints;
    const int kTargetCount = 1;

    // results
    int vertexEntryPointIndex = -1;
    int fragmentEntryPointIndex = -1;
    int computeEntryPointIndex = -1;
    ComPtr<slang::IComponentType> linkedProgram{};

    slang::ProgramLayout* _programLayout;
    inline bool HasVertexEntryPoint() const { return vertexEntryPointIndex != -1; }
    inline bool HasFragmentEntryPoint() const { return fragmentEntryPointIndex != -1; }
    inline bool HasComputeEntryPoint() const { return computeEntryPointIndex != -1; }

private:
    slang::IGlobalSession* globalSession;

public:
    static void DiagnoseIfNeeded(slang::IBlob* diagnostics)
    {
        if (diagnostics)
        {
            spdlog::error("{}", (const char*)diagnostics->getBufferPointer());
        }
    }

    Result CollectEntryPointMetadata(slang::IComponentType* program, int targetIndex, int entryPointCount)
    {
        metadataForEntryPoints.resize(entryPointCount);
        for (int entryPointIndex = 0; entryPointIndex < entryPointCount; entryPointIndex++)
        {
            ComPtr<slang::IMetadata> entryPointMetadata;
            ComPtr<slang::IBlob> diagnostics;
            SLANG_RETURN_ON_FAIL(program->getEntryPointMetadata(
                entryPointIndex,
                targetIndex,
                entryPointMetadata.writeRef(),
                diagnostics.writeRef()
            ));

            DiagnoseIfNeeded(diagnostics);

            metadataForEntryPoints[entryPointIndex] = entryPointMetadata;
        }
        return SLANG_OK;
    }

    struct StringBlob : public slang::IBlob
    {
        std::string string;

        SLANG_NO_THROW void const* getBufferPointer() override { return string.c_str(); }
        SLANG_NO_THROW size_t getBufferSize() override { return string.size(); }

        SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) override
        {
            *outObject = nullptr;
            return SLANG_OK;
        }
        SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return 0; }
        SLANG_NO_THROW uint32_t SLANG_MCALL release() override { return 0; }
    };

    slang::IModule* LoadFeatureModule(slang::ISession* session, const std::string& shaderName, std::string featureName)
    {
        auto srcString = fmt::format("export static const bool {} = true;", featureName);
        std::string moduleName = fmt::format("{}-{}", shaderName, featureName);
        std::string syntheticModulePath = fmt::format("_syntheticPath/{}.slang", moduleName);
        slang::IModule* toggleModule = session->loadModuleFromSourceString(
            moduleName.c_str(),          // module name
            syntheticModulePath.c_str(), // synthetic module path
            srcString.c_str()
        ); // module source content

        return toggleModule;
    }

    Result CompileAndReflectProgram(
        slang::ISession* session,
        const char* shaderName,
        Gfx::PipelineInfo& outPipelineInfo,
        Gfx::PipelineConfig& outPipelineConfig,
        const std::vector<std::string>& enabledFeatures
    )
    {
        globalSession = session->getGlobalSession();
        ComPtr<slang::IBlob> diagnostics;
        Result result = SLANG_OK;

        ComPtr<slang::IModule> module;
        module = session->loadModule(shaderName, diagnostics.writeRef());

        DiagnoseIfNeeded(diagnostics);
        if (!module)
            return SLANG_FAIL;

        std::vector<slang::IComponentType*> componentsToLink;

        for (auto& enabledFeature : enabledFeatures)
        {
            componentsToLink.push_back(LoadFeatureModule(session, shaderName, enabledFeature));
        }

        int definedEntryPointCount = module->getDefinedEntryPointCount();
        for (int i = 0; i < definedEntryPointCount; i++)
        {
            ComPtr<slang::IEntryPoint> entryPoint;
            SLANG_RETURN_ON_FAIL(module->getDefinedEntryPoint(i, entryPoint.writeRef()));

            auto attribute = entryPoint->getFunctionReflection()->findAttributeByName(globalSession, "shader");
            size_t size;
            const char* f = attribute->getArgumentValueString(0, &size);
            std::string name = std::string(f, size);
            if (name == "\"fragment\"")
            {
                outPipelineInfo.fragmentShaderName = entryPoint->getFunctionReflection()->getName();
                fragmentEntryPointIndex = i;
            }
            else if (name == "\"vertex\"")
            {
                outPipelineInfo.vertexShaderName = entryPoint->getFunctionReflection()->getName();
                vertexEntryPointIndex = i;
            }
            else if (name == "\"compute\"")
            {
                outPipelineInfo.computeShaderName = entryPoint->getFunctionReflection()->getName();
                computeEntryPointIndex = i;
            }

            componentsToLink.push_back(ComPtr<slang::IComponentType>(entryPoint.get()));
        }

        // ### Composing and Linking
        //

        ComPtr<slang::IComponentType> composed;
        result = session->createCompositeComponentType(
            (slang::IComponentType**)componentsToLink.data(),
            componentsToLink.size(),
            composed.writeRef(),
            diagnostics.writeRef()
        );
        DiagnoseIfNeeded(diagnostics);
        SLANG_RETURN_ON_FAIL(result);

        result = composed->link(linkedProgram.writeRef(), diagnostics.writeRef());
        DiagnoseIfNeeded(diagnostics);
        SLANG_RETURN_ON_FAIL(result);

        slang::ProgramLayout* programLayout = linkedProgram->getLayout(0, diagnostics.writeRef());
        _programLayout = programLayout;
        DiagnoseIfNeeded(diagnostics);
        if (!programLayout)
        {
            result = SLANG_FAIL;
            return result;
        }

        SLANG_RETURN_ON_FAIL(CollectEntryPointMetadata(linkedProgram, 0, definedEntryPointCount));

        outPipelineInfo = {};
        CollectSets(programLayout->getGlobalParamsVarLayout(), outPipelineInfo);
        if (HasVertexEntryPoint())
            CollectVertexInput(outPipelineInfo);
        if (HasFragmentEntryPoint())
            CollectFragmentOutput(outPipelineInfo);
        for (auto& set : outPipelineInfo.descriptorSets)
            set.UpdateBindingIndex();
        outPipelineInfo.name = shaderName;

        // retrive pipelineConfig
        {
            auto filePath = module->getFilePath();
            std::ifstream file(filePath);

            if (file)
            {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::stringstream yamlConfig = GetYAML(buffer);
                ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.str()));
                outPipelineConfig = MapShaderConfig(tree, outPipelineInfo);
            }
        }
        return result;
    }

    Gfx::ShaderStageFlags MapSlangStageMask(slang::ParameterCategory layoutUnit, int space, int offset)
    {
        Gfx::ShaderStageFlags stages = Gfx::ShaderStage::None;
        auto entryPointCount = metadataForEntryPoints.size();
        unsigned mask = 0;
        for (int i = 0; i < entryPointCount; ++i)
        {
            bool isUsed = false;
            metadataForEntryPoints[i]
                ->isParameterLocationUsed(SlangParameterCategory(layoutUnit), space, offset, isUsed);
            if (isUsed)
            {
                auto entryPointStage = _programLayout->getEntryPointByIndex(i)->getStage();

                mask |= 1 << unsigned(entryPointStage);
            }
        }

        if (mask & 1 << SLANG_STAGE_VERTEX)
            stages |= Gfx::ShaderStage::Vertex;
        if (mask & 1 << SLANG_STAGE_FRAGMENT)
            stages |= Gfx::ShaderStage::Fragment;
        if (mask & 1 << SLANG_STAGE_COMPUTE)
            stages |= Gfx::ShaderStage::Compute;
        if (mask == 0)
        {
            for (int i = 0; i < entryPointCount; ++i)
            {
                bool isUsed = false;
                metadataForEntryPoints[i]
                    ->isParameterLocationUsed(SlangParameterCategory(layoutUnit), space, offset, isUsed);
                if (isUsed)
                {
                    auto entryPointStage = _programLayout->getEntryPointByIndex(i)->getStage();

                    mask |= 1 << unsigned(entryPointStage);
                }
            }
        }
        return stages;
    }

    Gfx::DescriptorType MapSlangDescriptorType(
        slang::VariableLayoutReflection* variableLayout, SlangResourceShape shape
    )
    {
        Gfx::DescriptorType type = Gfx::DescriptorType::Invalid;

        auto typeLayout = variableLayout->getTypeLayout();
        auto access = typeLayout->getResourceAccess();
        ASSERT(typeLayout->getBindingRangeCount() == 1);
        {
#define MAP_SLANG_DESCRIPTOR_TYPE_CASE(from, to)                                                                       \
    case slang::BindingType::from: return Gfx::DescriptorType::to;

            auto rangeType = typeLayout->getBindingRangeType(0);

            switch (rangeType)
            {
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(CombinedTextureSampler, CombinedImageSampler);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(Sampler, Sampler);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(MutableTexture, StorageImage);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(Texture, SampledImage);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(TypedBuffer, UniformTexelBuffer);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(MutableTypedBuffer, StorageTexelBuffer);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(ConstantBuffer, UniformBuffer);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(ParameterBlock, UniformBuffer);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(RawBuffer, StorageBuffer);
                MAP_SLANG_DESCRIPTOR_TYPE_CASE(MutableRawBuffer, StorageBuffer);
                default:
                    {
                        ASSERT(0 && "Not Handled");
                        type = Gfx::DescriptorType::Invalid;
                        break;
                    }
            }
        }
        return type;
    }

    Gfx::TextureType MapSlangTextureType(SlangResourceShape shape)
    {
        Gfx::TextureType type = Gfx::TextureType::Invalid;

        if (shape == SlangResourceShape::SLANG_TEXTURE_2D)
            type = Gfx::TextureType::Tex2D;
        else if (shape == SlangResourceShape::SLANG_TEXTURE_3D)
            type = Gfx::TextureType::Tex3D;
        else if (shape == SlangResourceShape::SLANG_TEXTURE_CUBE)
            type = Gfx::TextureType::TexCube;
        else if (shape == SlangResourceShape::SLANG_RESOURCE_NONE)
            type = Gfx::TextureType::Invalid;
        else
            ASSERT(0 && "Not Handled");

        return type;
    }

    int AddSamplerConfig(Gfx::PipelineInfo::DescriptorSet& set, slang::VariableLayoutReflection* variableLayout)
    {
        Gfx::PipelineInfo::SamplerConfig config{};
        std::string name = variableLayout->getName();
        bool pointFilter = Utils::strContians(Utils::strToLower(name), "point");

        bool clampSampleToBorder = Utils::strContians(Utils::strToLower(name), "clamptoborder");
        bool clampSample = Utils::strContians(Utils::strToLower(name), "clamp");

        std::string samplerTypeName = variableLayout->getType()->getName();
        if (samplerTypeName == "SamplerComparisonState")
            config.enbaleCompare = true;
        else
            config.enbaleCompare = false;
        config.anisotropic = false;

        if (clampSampleToBorder)
            config.addressModeU = config.addressModeV = config.addressModeW = Gfx::SamplerAddressMode::ClampToBorder;
        else if (clampSample)
            config.addressModeU = config.addressModeV = config.addressModeW = Gfx::SamplerAddressMode::ClampToEdge;
        else
            config.addressModeU = config.addressModeV = config.addressModeW = Gfx::SamplerAddressMode::Repeat;

        if (pointFilter)
        {
            config.minFilter = Gfx::FilterMode::Nearest;
            config.magFilter = Gfx::FilterMode::Nearest;
        }
        else
        {
            config.minFilter = Gfx::FilterMode::Linear;
            config.magFilter = Gfx::FilterMode::Linear;
        }

        return set.AddSamplerConfig(config);
    }

    Gfx::PipelineInfo::MemberDataType MapSlangScalarType(slang::TypeReflection::ScalarType type)
    {
        switch (type)
        {
            case slang::TypeReflection::ScalarType::Float32:
            case slang::TypeReflection::ScalarType::Float16: return Gfx::PipelineInfo::MemberDataType::Float;
            case slang::TypeReflection::ScalarType::Int32:
            case slang::TypeReflection::ScalarType::Int16:
            case slang::TypeReflection::ScalarType::Int8: return Gfx::PipelineInfo::MemberDataType::Int;
            case slang::TypeReflection::ScalarType::UInt32:
            case slang::TypeReflection::ScalarType::UInt64: return Gfx::PipelineInfo::MemberDataType::UInt;
            default: ASSERT(0 && "Not Handled");
        }

        return Gfx::PipelineInfo::MemberDataType::Float;
    }

    std::vector<Gfx::PipelineInfo::BufferMember> CollectBufferMembers(slang::VariableLayoutReflection* variableLayout)
    {
        std::vector<Gfx::PipelineInfo::BufferMember> members{};
        auto typeLayout = variableLayout->getTypeLayout();

        auto fieldCount = typeLayout->getFieldCount();
        for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
        {
            auto field = typeLayout->getFieldByIndex(fieldIndex);
            auto fieldTypeLayout = field->getTypeLayout();
            auto fieldKind = fieldTypeLayout->getKind();
            auto kind = fieldTypeLayout->getKind();
            int size = fieldTypeLayout->getSize();
            switch (fieldKind)
            {
                case slang::TypeReflection::Kind::Struct:
                case slang::TypeReflection::Kind::Array:
                    {
                        if (size != 0)
                        {
                            Gfx::PipelineInfo::BufferMember member{};
                            member.name = field->getName();
                            member.count = 0;
                            if (kind == slang::TypeReflection::Kind::Struct)
                                member.type = Gfx::PipelineInfo::MemberDataType::Structure;
                            else // Array
                            {
                                member.count = fieldTypeLayout->getElementCount();
                                auto elementKind = fieldTypeLayout->getElementTypeLayout()->getKind();
                                if (elementKind == slang::TypeReflection::Kind::Struct)
                                {
                                    member.type = Gfx::PipelineInfo::MemberDataType::Structure;
                                }
                                else if (elementKind == slang::TypeReflection::Kind::Matrix)
                                {
                                    auto elementScalarType = fieldTypeLayout->getElementTypeLayout()->getScalarType();
                                    member.type = MapSlangScalarType(elementScalarType);
                                }
                                else
                                {
                                    member.type = MapSlangScalarType(fieldTypeLayout->getScalarType());
                                }
                            }
                            member.columnCount = 1;
                            member.rowCount = 1;                // as element count when type is a Vector
                            member.offset = field->getOffset(); // byte offset in it's containning struct
                            member.byteSize = size;

                            members.push_back(member);
                        }
                        break;
                    }

                case slang::TypeReflection::Kind::Matrix:
                case slang::TypeReflection::Kind::Vector:
                case slang::TypeReflection::Kind::Scalar:
                    {
                        Gfx::PipelineInfo::BufferMember member;
                        member.name = field->getName();
                        member.count = 0;
                        member.columnCount = 1;
                        member.rowCount = 1; // as element count when type is a Vector
                        member.type = MapSlangScalarType(fieldTypeLayout->getScalarType());
                        if (kind == slang::TypeReflection::Kind::Matrix)
                        {
                            member.columnCount = fieldTypeLayout->getColumnCount();
                            member.rowCount = fieldTypeLayout->getRowCount();
                        }
                        else if (kind == slang::TypeReflection::Kind::Vector)
                        {
                            member.rowCount = fieldTypeLayout->getElementCount();
                        }
                        member.offset = field->getOffset(); // byte offset in it's containning struct
                        member.byteSize = size;

                        members.push_back(member);
                        break;
                    }
                default: break;
            }
        }

        return members;
    }

    void CollectBindings(
        slang::VariableLayoutReflection* variableLayout,
        slang::VariableLayoutReflection* container,
        Gfx::PipelineInfo::DescriptorSet& set,
        int parentBinding,
        std::vector<Gfx::PipelineInfo::Binding>& outBindings
    )
    {
        auto typeLayout = variableLayout->getTypeLayout();
        auto kind = typeLayout->getKind();
        auto bindingOffset =
            variableLayout->getOffset((SlangParameterCategory)slang::ParameterCategory::DescriptorTableSlot);
        auto currentBinding = parentBinding + bindingOffset;

        switch (kind)
        {
            case slang::TypeReflection::Kind::SamplerState:
                {
                    Gfx::PipelineInfo::Binding binding{};
                    binding.name = variableLayout->getName();
                    binding.shaderBindingHandle = Gfx::ShaderBindingHandle(binding.name);
                    binding.bindingNum = currentBinding;
                    binding.descriptorCount = 1; // TODO array binding
                    binding.stages = MapSlangStageMask(slang::DescriptorTableSlot, set.setNum, currentBinding);
                    binding.descriptorType = MapSlangDescriptorType(variableLayout, typeLayout->getResourceShape());
                    binding.textureType = MapSlangTextureType(typeLayout->getResourceShape());
                    binding.bufferMembers = {};
                    binding.byteSize = 0;
                    binding.samplerIndex = AddSamplerConfig(set, variableLayout);

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

                    outBindings.push_back(binding);

                    break;
                }
            case slang::TypeReflection::Kind::Resource:
                {
                    Gfx::PipelineInfo::Binding binding{};
                    binding.name = variableLayout->getName();
                    binding.shaderBindingHandle = Gfx::ShaderBindingHandle(binding.name);
                    binding.bindingNum = currentBinding;
                    binding.descriptorCount = 1; // TODO array binding
                    binding.stages = MapSlangStageMask(slang::DescriptorTableSlot, set.setNum, currentBinding);
                    binding.descriptorType = MapSlangDescriptorType(variableLayout, typeLayout->getResourceShape());
                    if (binding.descriptorType == Gfx::DescriptorType::CombinedImageSampler ||
                        binding.descriptorType == Gfx::DescriptorType::SampledImage ||
                        binding.descriptorType == Gfx::DescriptorType::StorageImage)
                        binding.textureType = MapSlangTextureType(typeLayout->getResourceShape());
                    else
                        binding.textureType = Gfx::TextureType::Invalid;
                    binding.bufferMembers = {};
                    binding.byteSize = 0;
                    binding.samplerIndex = AddSamplerConfig(set, variableLayout);

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

                    outBindings.push_back(binding);

                    break;
                }
            case slang::TypeReflection::Kind::Struct:
                {
                    auto fieldCount = typeLayout->getFieldCount();
                    for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                    {
                        CollectBindings(
                            typeLayout->getFieldByIndex(fieldIndex),
                            nullptr,
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
                        Gfx::PipelineInfo::Binding binding{};
                        binding.name = variableLayout->getName();
                        binding.shaderBindingHandle = Gfx::ShaderBindingHandle(binding.name);
                        binding.bindingNum = currentBinding;
                        binding.descriptorCount = 1; // TODO array binding
                        binding.stages = MapSlangStageMask(
                            slang::ParameterCategory::DescriptorTableSlot,
                            set.setNum,
                            currentBinding
                        );
                        binding.descriptorType =
                            MapSlangDescriptorType(variableLayout, variableLayout->getType()->getResourceShape());
                        binding.textureType = Gfx::TextureType::Invalid;
                        binding.bufferMembers = CollectBufferMembers(elementVarLayout);
                        binding.byteSize = size;
                        binding.samplerIndex = -1;

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

                        outBindings.push_back(binding);
                    }
                    CollectBindings(elementVarLayout, variableLayout, set, parentBinding + bindingOffset, outBindings);
                    break;
                }
                // case slang::TypeReflection::Kind::Array:
                //     ASSERT(false && "Not Implemented");

            default:
                {
                    break;
                }
        }
    }

    void AccessSet(slang::VariableLayoutReflection* setLayoutReflection, Gfx::PipelineInfo& pipelineInfo)
    {
        Gfx::PipelineInfo::DescriptorSet set{};
        set.setNum = setLayoutReflection->getOffset((SlangParameterCategory)slang::SubElementRegisterSpace);
        CollectBindings(setLayoutReflection, nullptr, set, 0, set.bindings);

        set.name = setLayoutReflection->getName();
        pipelineInfo.descriptorSets.push_back(set);
    }

    Gfx::GfxFormat MapSlangFormat(std::string_view str)
    {
        if (str == "rgba8")
        {
            return Gfx::GfxFormat::R8G8B8A8_UNorm;
        }

        ASSERT(0 && "Not Handled");
        return Gfx::GfxFormat::Invalid;
    }

    void CollectVertexVaryingInput(
        slang::VariableLayoutReflection* variableLayout,
        int locationOffset,
        std::vector<Gfx::PipelineInfo::VertexAttribute>& outVertexAttributes
    )
    {
        auto category = variableLayout->getCategory();
        if (category == slang::ParameterCategory::VaryingInput)
        {
            auto kind = variableLayout->getType()->getKind();

            if (kind == slang::TypeReflection::Kind::Struct)
            {
                auto typeLayout = variableLayout->getTypeLayout();
                auto fieldCount = typeLayout->getFieldCount();
                auto offset = variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT);
                for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                {
                    CollectVertexVaryingInput(
                        typeLayout->getFieldByIndex(fieldIndex),
                        locationOffset + offset,
                        outVertexAttributes
                    );
                }
            }
            else if (kind == slang::TypeReflection::Kind::Vector)
            {
                Gfx::PipelineInfo::VertexAttribute vertexAttribute;

                vertexAttribute.name = variableLayout->getName();
                vertexAttribute.location =
                    locationOffset + variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT);

                bool used = false;
                metadataForEntryPoints[vertexEntryPointIndex]
                    ->isParameterLocationUsed(SLANG_PARAMETER_CATEGORY_VERTEX_INPUT, 0, vertexAttribute.location, used);
                if (!used)
                    return;

                const char* semanticName = variableLayout->getSemanticName();
                if (semanticName)
                {
                    vertexAttribute.semanticName = MapVertexAttributeSemantics(semanticName);
                    vertexAttribute.semanticIndex = variableLayout->getSemanticIndex();
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
                    vertexAttribute.semanticName =
                        defaultSemantics[outVertexAttributes.size() < 6 ? outVertexAttributes.size() : 0];
                    vertexAttribute.semanticIndex = 0;
                }

                auto byteSizeAttribute = variableLayout->getVariable()->findAttributeByName(globalSession, "format");
                auto elementCount = variableLayout->getType()->getElementCount();
                if (byteSizeAttribute)
                {
                    size_t strLen = 0;
                    const char* str = byteSizeAttribute->getArgumentValueString(0, &strLen);
                    std::string_view strView(str + 1, str + strLen - 1);
                    vertexAttribute.format = MapSlangFormat(strView);
                    if (vertexAttribute.format == Gfx::GfxFormat::Invalid)
                    {
                        spdlog::warn("provided shader UnderlyingFormat type is not supported, fall back to "
                                     "R32G32B32A32_SFloat");
                        vertexAttribute.format = Gfx::GfxFormat::R32G32B32A32_SFloat;
                    }
                }
                else
                {
                    if (elementCount == 1)
                        vertexAttribute.format = Gfx::GfxFormat::R32_SFloat;
                    else if (elementCount == 2)
                        vertexAttribute.format = Gfx::GfxFormat::R32G32_SFloat;
                    else if (elementCount == 3)
                        vertexAttribute.format = Gfx::GfxFormat::R32G32B32_SFloat;
                    else if (elementCount == 4)
                        vertexAttribute.format = Gfx::GfxFormat::R32G32B32A32_SFloat;
                }

                vertexAttribute.size = Gfx::MapGfxFormatToByteSize(vertexAttribute.format);

                outVertexAttributes.push_back(vertexAttribute);
            }
            else
                ASSERT(0 && "Not Handled");
        }
    }

    void CollectFragmentOutput(
        slang::VariableLayoutReflection* variableLayout,
        int locationOffset,
        std::vector<Gfx::PipelineInfo::FragmentOutput>& outFragmentOutputs
    )
    {
        auto category = variableLayout->getCategory();
        if (category == slang::ParameterCategory::VaryingOutput)
        {
            auto kind = variableLayout->getTypeLayout()->getKind();

            if (kind == slang::TypeReflection::Kind::Struct)
            {
                auto typeLayout = variableLayout->getTypeLayout();
                auto fieldCount = typeLayout->getFieldCount();
                auto offset = variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_OUTPUT);
                for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                {
                    CollectFragmentOutput(
                        typeLayout->getFieldByIndex(fieldIndex),
                        locationOffset + offset,
                        outFragmentOutputs
                    );
                }
            }
            else if (kind == slang::TypeReflection::Kind::Vector)
            {
                Gfx::PipelineInfo::FragmentOutput fragmentAttribute{};

                fragmentAttribute.location =
                    locationOffset + variableLayout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_OUTPUT);

                auto byteSizeAttribute = variableLayout->getType()->findAttributeByName("format");
                auto elementCount = variableLayout->getType()->getElementCount();
                if (byteSizeAttribute)
                {
                    size_t strLen = 0;
                    const char* str = byteSizeAttribute->getArgumentValueString(0, &strLen);
                    std::string_view strView(str + 1, str + strLen - 1);
                    fragmentAttribute.format = MapSlangFormat(strView);
                    if (fragmentAttribute.format == Gfx::GfxFormat::Invalid)
                    {
                        spdlog::warn("provided shader UnderlyingFormat type is not supported, fall back to "
                                     "R32G32B32A32_SFloat");
                        fragmentAttribute.format = Gfx::GfxFormat::R32G32B32A32_SFloat;
                    }
                }
                else
                {
                    if (elementCount == 1)
                        fragmentAttribute.format = Gfx::GfxFormat::R32_SFloat;
                    else if (elementCount == 2)
                        fragmentAttribute.format = Gfx::GfxFormat::R32G32_SFloat;
                    else if (elementCount == 3)
                        fragmentAttribute.format = Gfx::GfxFormat::R32G32B32_SFloat;
                    else if (elementCount == 4)
                        fragmentAttribute.format = Gfx::GfxFormat::R32G32B32A32_SFloat;
                }

                outFragmentOutputs.push_back(fragmentAttribute);
            }
            else
                ASSERT(0 && "Not Handled");
        }
    }

    void CollectFragmentOutput(Gfx::PipelineInfo& outPipelineInfo)
    {
        auto fragmentEntryPointReflection = linkedProgram->getLayout()->getEntryPointByIndex(fragmentEntryPointIndex);
        auto resultVarLayout = fragmentEntryPointReflection->getResultVarLayout();
        CollectFragmentOutput(resultVarLayout, 0, outPipelineInfo.fragmentOutputs);
    }

    void CollectVertexInput(Gfx::PipelineInfo& outPipelineInfo)
    {
        std::vector<Gfx::PipelineInfo::VertexAttribute> vertexAttributes{};
        auto vertexEntryPointReflection = linkedProgram->getLayout()->getEntryPointByIndex(vertexEntryPointIndex);

        for (int parameterIndex = 0; parameterIndex < vertexEntryPointReflection->getParameterCount(); ++parameterIndex)
        {
            auto param = vertexEntryPointReflection->getParameterByIndex(parameterIndex);
            CollectVertexVaryingInput(param, 0, outPipelineInfo.vertexInputs);
        }
    }

    void CollectSets(slang::VariableLayoutReflection* scopeVarLayout, Gfx::PipelineInfo& outPipelineInfo)
    {
        auto scopeTypeLayout = scopeVarLayout->getTypeLayout();
        const auto& kind = scopeTypeLayout->getKind();
        auto ProcessAsPushConstant = [&](slang::VariableLayoutReflection* var)
        {
            if (var->getCategory() == slang::ParameterCategory::PushConstantBuffer)
            {
                Gfx::PipelineInfo::PushConstant pushConstant;
                pushConstant.stages = Gfx::ShaderStage::Vertex |
                                      Gfx::ShaderStage::Fragment; // https://github.com/shader-slang/slang/issues/5685
                // push constant not supported to query yet
                pushConstant.size = var->getTypeLayout()->getElementTypeLayout()->getSize();
                outPipelineInfo.pushConstants.push_back(pushConstant);
            }
        };
        switch (kind)
        {
            // #### Parameters are Grouped Into a Structure
            //
            case slang::TypeReflection::Kind::Struct:
                {
                    int paramCount = scopeTypeLayout->getFieldCount();
                    for (int i = 0; i < paramCount; i++)
                    {
                        auto param = scopeTypeLayout->getFieldByIndex(i);
                        auto paramTypeLayout = param->getTypeLayout();
                        auto paramKind = paramTypeLayout->getKind();
                        std::string name = param->getName();
                        switch (paramKind)
                        {
                            case slang::TypeReflection::Kind::ParameterBlock:
                                {
                                    AccessSet(param, outPipelineInfo);
                                }
                                break;
                            case slang::TypeReflection::Kind::ConstantBuffer:
                                {
                                    ProcessAsPushConstant(param);
                                    break;
                                }
                            default: ASSERT(false && "Not handled");
                        }
                    }
                    break;
                }
            default: ASSERT(false && "Not handled"); break;
        }
    }

    Gfx::PipelineConfig MapShaderConfig(ryml::Tree& tree, Gfx::PipelineInfo& info)
    {
        Gfx::PipelineConfig::PipelineConfig_t config;
        config.color.blendConstants[0] = 1.0;
        config.color.blendConstants[1] = 1.0;
        config.color.blendConstants[2] = 1.0;
        config.color.blendConstants[3] = 1.0;

        ryml::NodeRef root = tree.rootref();
        if (root.empty())
            return config;

        root.get_if("interleaved", &info.isVertexInterleaved);
        config.depth.boundTestEnable = false;

        if (root.has_child("input"))
        {
            if (root["input"].is_map())
            {
                for (const ryml::NodeRef& iter : root["input"])
                {
                    std::string inputName(iter.key().begin(), iter.key().size());
                    if (iter.has_child("baseTypeSize"))
                    {
                        int size = 0;
                        iter.get_if("baseTypeSize", &size);
                    }
                }
            }
        }

        if (root.has_child("mask"))
        {

            if (root["mask"].is_seq()) // mask for multiple render target, to be implmented
            {
                throw std::runtime_error("not implmented");
                Gfx::ColorBlendAttachmentState state;
                config.color.blends.push_back(state);
            }
            else // mask for the first render target
            {
                std::string val;
                root["mask"] >> val;
                Gfx::ColorBlendAttachmentState state{};
                state.colorWriteMask = Utils::MapColorMask(val);
                config.color.blends.push_back(state);
            }
        }

        if (root.has_child("polygonMode"))
        {
            std::string val;
            root["polygonMode"] >> val;
            config.polygonMode = Utils::MapPolygonMode(val);
        }

        if (root.has_child("topology"))
        {
            std::string val;
            root["topology"] >> val;
            config.topology = Utils::MapTopology(val);
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
                        Gfx::ColorBlendAttachmentState* state;
                        if (i < config.color.blends.size())
                        {
                            state = &config.color.blends[i];
                        }
                        else
                        {
                            config.color.blends.push_back(Gfx::ColorBlendAttachmentState{});
                            state = &config.color.blends.back();
                        }

                        std::string val;
                        iter >> val;

                        std::regex blendWithColorPattern("(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)");
                        std::regex blendPattern("(\\w+)\\s+(\\w+)");
                        std::cmatch m;
                        if (std::regex_match(val.c_str(), m, blendWithColorPattern))
                        {
                            state->blendEnable = true;
                            std::string srcColorBlendFactor = m[1].str();
                            std::string srcAlphaBlendFactor = m[2].str();
                            std::string dstColorBlendFactor = m[3].str();
                            std::string dstAlphaBlendFactor = m[4].str();

                            state->srcColorBlendFactor = Utils::MapBlendFactor(srcColorBlendFactor);
                            state->srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                            state->dstColorBlendFactor = Utils::MapBlendFactor(dstColorBlendFactor);
                            state->dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
                        }
                        else if (std::regex_match(val.c_str(), m, blendPattern))
                        {
                            state->blendEnable = true;
                            std::string srcAlphaBlendFactor = m[1].str();
                            std::string dstAlphaBlendFactor = m[2].str();
                            state->srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                            state->dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
                            state->srcColorBlendFactor = state->srcAlphaBlendFactor;
                            state->dstColorBlendFactor = state->dstAlphaBlendFactor;

                            if (state->srcAlphaBlendFactor == Gfx::BlendFactor::One &&
                                state->dstAlphaBlendFactor == Gfx::BlendFactor::Zero)
                                state->blendEnable = false;
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
                uint32_t i = 0;
                for (const ryml::NodeRef& iter : root["blendOp"])
                {
                    if (i >= config.color.blends.size())
                        break;

                    std::regex alphaOnly("(\\w+)");
                    std::regex withColor("(\\w+)\\s+(\\w+)");

                    std::string val;
                    iter >> val;

                    std::cmatch m;
                    if (std::regex_match(val.c_str(), m, withColor))
                    {
                        config.color.blends[i].colorBlendOp = Utils::MapBlendOp(m[1].str());
                        config.color.blends[i].alphaBlendOp = Utils::MapBlendOp(m[2].str());
                    }
                    else if (std::regex_match(val.c_str(), m, alphaOnly))
                    {
                        config.color.blends[i].alphaBlendOp = Utils::MapBlendOp(m[1].str());
                        config.color.blends[i].colorBlendOp = config.color.blends[i].alphaBlendOp;
                    }
                }
            }
        }

        if (root.has_child("cull"))
        {
            std::string val;
            auto cull = root["cull"];
            if (cull.is_keyval())
            {
                root["cull"] >> val;
                config.cullMode = Utils::MapCullMode(val);
            }
        }

        if (root.has_child("depth"))
        {
            std::string val;
            auto depth = root["depth"];
            depth.get_if("testEnable", &config.depth.testEnable);
            depth.get_if("writeEnable", &config.depth.writeEnable);
            depth.get_if("compOp", &val, std::string("lessOrEqual"));
            config.depth.compOp = Utils::MapCompareOp(val);
            depth.get_if("boundTestEnable", &config.depth.boundTestEnable);
            depth.get_if("minBounds", &config.depth.minBounds);
            depth.get_if("maxBounds", &config.depth.maxBounds);
        }

        if (root.has_child("stencil"))
        {
            std::string val;
            auto stencil = root["stencil"];
            stencil.get_if("testEnable", &config.stencil.testEnable);

            if (!stencil.has_child("front") && !stencil.has_child("back"))
            {
                stencil.get_if("failOp", &val, std::string("keep"));
                config.stencil.front.failOp = config.stencil.back.failOp = Utils::MapStencilOp(val);
                stencil.get_if("passOp", &val, std::string("keep"));
                config.stencil.front.passOp = config.stencil.back.passOp = Utils::MapStencilOp(val);
                stencil.get_if("depthFailOp", &val, std::string("keep"));
                config.stencil.front.depthFailOp = config.stencil.back.depthFailOp = Utils::MapStencilOp(val);
                stencil.get_if("compareOp", &val, std::string("never"));
                config.stencil.front.compareOp = config.stencil.back.compareOp = Utils::MapCompareOp(val);
                stencil.get_if("compareMask", &config.stencil.front.compareMask);
                stencil.get_if("writeMask", &config.stencil.front.writeMask);
                stencil.get_if("reference", &config.stencil.front.reference);
                config.stencil.back.compareMask = config.stencil.front.compareMask;
                config.stencil.back.writeMask = config.stencil.front.writeMask;
                config.stencil.back.reference = config.stencil.front.reference;
            }

            if (stencil.has_child("front"))
            {
                auto front = stencil["front"];
                front.get_if("failOp", &val, std::string("keep"));
                config.stencil.front.failOp = Utils::MapStencilOp(val);
                front.get_if("passOp", &val, std::string("keep"));
                config.stencil.front.passOp = Utils::MapStencilOp(val);
                front.get_if("depthFailOp", &val, std::string("keep"));
                config.stencil.front.depthFailOp = Utils::MapStencilOp(val);
                front.get_if("compareOp", &val, std::string("never"));
                config.stencil.front.compareOp = Utils::MapCompareOp(val);
                front.get_if("compareMask", &config.stencil.front.compareMask);
                front.get_if("writeMask", &config.stencil.front.writeMask);
                front.get_if("reference", &config.stencil.front.reference);
            }

            if (stencil.has_child("back"))
            {
                auto back = stencil["back"];
                back.get_if("failOp", &val, std::string("keep"));
                config.stencil.back.failOp = Utils::MapStencilOp(val);
                back.get_if("passOp", &val, std::string("keep"));
                config.stencil.back.passOp = Utils::MapStencilOp(val);
                back.get_if("depthFailOp", &val, std::string("keep"));
                config.stencil.back.depthFailOp = Utils::MapStencilOp(val);
                back.get_if("compareOp", &val, std::string("never"));
                config.stencil.back.compareOp = Utils::MapCompareOp(val);
                back.get_if("compareMask", &config.stencil.back.compareMask);
                back.get_if("writeMask", &config.stencil.back.writeMask);
                back.get_if("reference", &config.stencil.back.reference);
            }
        }
        return config;
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
};
