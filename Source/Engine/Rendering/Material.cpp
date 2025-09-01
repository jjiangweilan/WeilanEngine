#include "Material.hpp"
#include "Core/FrameContext.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Libs/Assert.hpp"
#include "Rendering/ShaderLibrary.hpp"

DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");

Material::Material(std::string_view shaderName)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetShader(shaderName);
}

Material::Material(ObjPtr<Shader2> shader)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetShaderNoProtection(shader);
}

Material::Material() : shaderInUse(nullptr), shaderResource(nullptr)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetName("new material");
}

Material::~Material() {};

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    if (shaderResource != nullptr)
        shaderResource->Remove(param);
    SetDirty();
}

void Material::SetTexture(
    const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    // if texture is nullptr, redirect to nullptr implementation
    if (texture == nullptr)
    {
        SetTexture(param, (std::nullptr_t) nullptr);
        return;
    }

    auto iter = textureValues.find(param);
    bool same = false;
    if (iter != textureValues.end())
    {
        same = iter->second.Get() == texture;
    }
    if (same)
    {
        auto optionIter = textureImageViewOptions.find(param);
        if (optionIter != textureImageViewOptions.end())
        {
            same = imageViewOption == optionIter->second;
        }
    }
    if (same)
        return;

    SetDirty();
    SetTextureInternal(param, texture, imageViewOption);
}

void Material::SetTextureInternal(
    const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    textureValues[param] = texture;
    textureImageViewOptions[param] = imageViewOption;

    if (shaderResource != nullptr)
    {
        if (imageViewOption.has_value())
        {
            shaderResource->SetImage(param, &texture->GetGfxImage()->GetImageView(*imageViewOption));
        }
        else
        {
            shaderResource->SetImage(param, texture->GetGfxImage());
        }
    }
}

void Material::SetBuffer(const std::string& name, Gfx::Buffer* buffer)
{
    shaderResource->SetBuffer(name, buffer);
}

void Material::RebuildAllMaterials()
{
    auto materials = Object::GetObjectsOfType<Material>();
    for (Material* mat : materials)
    {
        if (mat->shaderResource)
        {
            mat->shaderResource->Clear();

            // copy to skip reset test
            auto copy = mat->textureValues;
            mat->textureValues.clear();
            for (auto& kv : copy)
            {
                if (kv.second != nullptr)
                    mat->SetTexture(kv.first, kv.second);
            }
            mat->ubo.buffer = nullptr;
            mat->ubo.dirty = true;
            mat->uploadNeeded = true;
        }
    }
}

void Material::SetTexture(
    const std::string& param, Gfx::Image* image, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    if (shaderResource != nullptr)
    {
        if (imageViewOption.has_value())
            shaderResource->SetImage(param, &image->GetImageView(*imageViewOption));
        else
            shaderResource->SetImage(param, image);
    }
}

void Material::SetMatrix(const std::string& name, const glm::mat4& value)
{
    SetMatrix("", name, value);
}

void Material::SetFloat(const std::string& name, float value)
{
    SetFloat("", name, value);
}

void Material::SetVector(const std::string& name, const glm::vec4& value)
{
    SetVector("", name, value);
}

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    auto iter = ubo.matrices.find(member);
    if (iter == ubo.matrices.end() || iter->second != value)
    {
        ubo.matrices[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        SetDirty();
    }
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    auto iter = ubo.floats.find(member);
    if (iter == ubo.floats.end() || iter->second != value)
    {
        ubo.floats[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        SetDirty();
    }
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    auto iter = ubo.vectors.find(member);
    if (iter == ubo.vectors.end() || iter->second != value)
    {
        ubo.vectors[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        SetDirty();
    }
}

glm::mat4 Material::GetMatrix(const std::string& param, const std::string& member)
{
    auto memIter = ubo.matrices.find(member);
    if (memIter != ubo.matrices.end())
    {
        return memIter->second;
    }
    return glm::mat4(0);
}

glm::vec4 Material::GetVector(const std::string& param, const std::string& member)
{
    auto memIter = ubo.vectors.find(member);
    if (memIter != ubo.vectors.end())
    {
        return memIter->second;
    }
    return glm::vec4(0);
}

Texture* Material::GetTexture(const std::string& param)
{
    auto iter = textureValues.find(param);
    if (iter != textureValues.end())
    {
        return iter->second;
    }

    return nullptr;
}

float Material::GetFloat(const std::string& param, const std::string& member)
{
    auto memIter = ubo.floats.find(member);
    if (memIter != ubo.floats.end())
    {
        return memIter->second;
    }
    return 0;
}
void Material::SetShader(Shader2* shader)
{
    if (this->shaderInUse.Get() != shader)
    {
        needRequestNewShader = false;
        this->shaderName = shader->GetName();
        SetShaderNoProtection(shader);
        SetDirty();
    }
}

void Material::SetShader(std::string_view shaderName)
{
    if (shaderInUse == nullptr || this->shaderName != shaderName)
    {
        auto shaderFeatures = &ShaderLibrary::QueryShaderFeatures(shaderName.data());

        if (shaderFeatures)
        {
            auto perm = shaderFeatures->GetPermutation(enabledFeatures);
            auto shader = ShaderLibrary::GetShader(shaderName.data(), perm);

            if (shader == nullptr)
                return;

            needRequestNewShader = false;
            this->shaderFeatures = shaderFeatures;
            this->shaderName = shaderName;
            SetShaderNoProtection(shader);
            SetDirty();
        }
    }
}

void Material::SetShaderNoProtection(ObjPtr<Shader2> shaderProgram)
{
    this->shaderInUse = shaderProgram;
    uploadNeeded = true;
    shaderConfig = shaderInUse->GetShaderProgram()->GetDefaultShaderConfig();
}

void Material::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shader", shaderInUse);
    s->Serialize("ubo", ubo);
    s->Serialize("textureValues", textureValues);
    std::vector<std::string> enabledFeatureVec(enabledFeatures.begin(), enabledFeatures.end());
    s->Serialize("enabledFeature", enabledFeatureVec);
    s->Serialize("overrideShaderConfig", overrideShaderConfig);
    s->Serialize("shaderConfig", overrideShaderConfig ? shaderConfig.ToJson() : nlohmann::json());
    SERIALIZE(s, shaderName);
}

std::unique_ptr<Asset> Material::Clone()
{
    return nullptr;
    // return std::unique_ptr<Material>(new Material(*this));
}

Gfx::ShaderResource* Material::ValidateGetShaderResource()
{
    if (shaderInUse == nullptr)
    {
        spdlog::warn("Shader is not set in material");
        return nullptr;
    }

    if (uploadNeeded)
    {
        UploadDataToGPU(shaderInUse->GetShaderProgram());
    }
    return shaderResource.get();
}

Gfx::ShaderProgram* Material::GetShaderProgram()
{
    if (needRequestNewShader)
    {
        if (!shaderFeatures)
        {
            shaderFeatures = &ShaderLibrary::QueryShaderFeatures(shaderName.data());
        }
        shaderInUse = ShaderLibrary::GetShader(shaderName.data(), shaderFeatures->GetPermutation(enabledFeatures));
        needRequestNewShader = false;
    }

    if (shaderInUse)
        return shaderInUse->GetShaderProgram();

    return nullptr;
}

void Material::EnableFeature(const std::string& name)
{
    if (!enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.emplace(name);
        SetDirty();
    }
}

void Material::DisableFeature(const std::string& name)
{
    if (enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.erase(name);
        SetDirty();
    }
}

void Material::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    // s->Deserialize("shader", shader);
    s->Deserialize("ubo", ubo);
    s->Deserialize("textureValues", textureValues);
    std::vector<std::string> enabledFeatureVec;
    s->Deserialize("enabledFeature", enabledFeatureVec);
    for (auto& f : enabledFeatureVec)
    {
        EnableFeature(f);
    }
    nlohmann::json shaderConfigJson;
    s->Deserialize("shaderConfig", shaderConfigJson);
    s->Deserialize("overrideShaderConfig", overrideShaderConfig);
    DESERIALIZE(s, shaderName);
    if (!shaderName.empty())
    {
        SetShader(shaderName);
    }
}

void Material::OnLoaded()
{
    for (auto& kv : textureValues)
    {
        SetTextureInternal(kv.first, kv.second, std::nullopt);
    }
}

void Material::UBO::Serialize(Serializer* ser) const
{
    ser->Serialize("floats", floats);
    ser->Serialize("vectors", vectors);
    ser->Serialize("matrices", matrices);
}

void Material::UBO::Deserialize(Serializer* ser)
{
    ser->Deserialize("floats", floats);
    ser->Deserialize("vectors", vectors);
    ser->Deserialize("matrices", matrices);
    dirty = true;
}

void Material::UploadDataToGPU(Gfx::ShaderProgram* shaderProgram)
{
    if (!shaderProgram)
        return;

    uploadNeeded = false;

    if (ubo.dirty)
    {
        ubo.dirty = false;
        const auto& pipelineInfo = shaderProgram->GetShaderInfo();
        auto descriptorSet = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material);
        if (descriptorSet == nullptr)
            return;

        auto binding = descriptorSet->GetBinding(0);
        if (binding != nullptr && binding->descriptorType == Gfx::DescriptorType::UniformBuffer)
        {
            // Create the buffer
            if (ubo.buffer == nullptr)
            {
                size_t size = binding->byteSize;
                std::unique_ptr<Gfx::Buffer> buffer = GetGfxDriver()->CreateBuffer({
                    .usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
                    .size = size,
                    .visibleInCPU = false,
                    .debugName = "Material Uniform Buffer",
                });

                auto uboBindingName = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material)->name;
                shaderResource->SetBuffer(uboBindingName, buffer.get());
                ubo.buffer = std::move(buffer);
            }

            auto bufSize = binding->byteSize;

            auto [tempBuf, tempUploadDataHandle] = GetStackAllocator().ScopedAllocate<unsigned char>(bufSize);

            ASSERT(binding->descriptorType == Gfx::DescriptorType::UniformBuffer && "UBO should be a structure");
            for (auto& member : binding->bufferMembers)
            {
                UploadDataToGPUInternal(pipelineInfo, member, tempBuf, bufSize);
            }
            GetGfxDriver()->UploadBuffer(*ubo.buffer, tempBuf, bufSize, 0);
        }
    }
}

void Material::UploadDataToGPUInternal(
    const Gfx::PipelineInfo& pipelineInfo,
    const Gfx::PipelineInfo::BufferMember& bufferDataDescription,
    uint8_t* buf,
    size_t bufSize
)
{
    size_t offset = bufferDataDescription.offset;
    ASSERT(!bufferDataDescription.IsArray());

    if (bufferDataDescription.IsVector())
    {
        ASSERT(bufferDataDescription.type == Gfx::PipelineInfo::MemberDataType::Float);
        {
            auto iter = ubo.vectors.find(bufferDataDescription.name);
            if (iter != ubo.vectors.end())
            {
                if (bufferDataDescription.rowCount == 3 || bufferDataDescription.rowCount == 4)
                {
                    ASSERT(offset + sizeof(glm::vec4) <= bufSize);
                    *((glm::vec4*)(buf + offset)) = iter->second;
                }
                else if (bufferDataDescription.rowCount == 2)
                {
                    ASSERT(offset + sizeof(glm::vec2) <= bufSize);
                    *((glm::vec2*)(buf + offset)) = glm::vec2(iter->second);
                }
            }
        }
    }
    else if (bufferDataDescription.IsMatrix())
    {
        auto iter = ubo.matrices.find(bufferDataDescription.name);
        if (iter != ubo.matrices.end())
        {
            ASSERT(bufferDataDescription.rowCount == 4 && bufferDataDescription.columnCount == 4);
            {
                ASSERT(offset + sizeof(glm::mat4) <= bufSize);
                *((glm::mat4*)(buf + offset)) = iter->second;
            }
        }
    }
    else // scatter type
    {
        switch (bufferDataDescription.type)
        {
            case Gfx::PipelineInfo::MemberDataType::Float:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(float) <= bufSize);
                        *((float*)(buf + offset)) = iter->second;
                    }
                    break;
                }
            case Gfx::PipelineInfo::MemberDataType::UInt:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(uint32_t) <= bufSize);
                        *((uint32_t*)(buf + offset)) = (uint32_t)iter->second;
                    }
                    break;
                }
            case Gfx::PipelineInfo::MemberDataType::Int:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(int32_t) <= bufSize);
                        *((int32_t*)(buf + offset)) = (int32_t)iter->second;
                    }
                    break;
                }
            case Gfx::PipelineInfo::MemberDataType::Structure:
                {
                    ASSERT(false && "Nested Structure not supported in material");
                    break;
                }
            default: ASSERT(0 && "Not Handled"); break;
        }
    }
}

const Gfx::PipelineConfig& Material::GetShaderConfig()
{
    if (overrideShaderConfig || shaderInUse == nullptr)
        return shaderConfig;
    else
    {
        Shader2* s = shaderInUse;
        return s->GetShaderProgram()->GetDefaultShaderConfig();
    }
}

int Material::GetSet(Gfx::DescriptorSetSemantics semantics) const
{
    if (shaderInUse)
    {
        auto set = shaderInUse->GetShaderProgram()->GetShaderInfo().GetDescriptorSet(semantics);
        if (set)
            return set->setNum;
    }

    return -1;
}

int Material::GetSet(const std::string& name) const
{
    if (shaderInUse)
    {
        auto set = shaderInUse->GetShaderProgram()->GetShaderInfo().GetDescriptorSet(name);
        if (set)
            return set->setNum;
    }

    return -1;
}

void Material::CopyProperties(Material& other)
{
    if (ubo.floats != other.ubo.floats)
    {
        ubo.floats = other.ubo.floats;
        ubo.dirty = true;
        uploadNeeded = true;
    }

    if (ubo.vectors != other.ubo.vectors)
    {
        ubo.vectors = other.ubo.vectors;
        ubo.dirty = true;
        uploadNeeded = true;
    }

    if (ubo.matrices != other.ubo.matrices)
    {
        ubo.matrices = other.ubo.matrices;
        ubo.dirty = true;
        uploadNeeded = true;
    }
}
