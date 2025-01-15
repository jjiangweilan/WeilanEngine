#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Libs/Assert.hpp"
#include "Rendering/ShaderLibrary.hpp"

DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");

Material::Material(std::string_view shaderName)
{
    SetShader(shaderName);
}

Material::Material(ObjPtr<Shader2> shader)
{
    SetShaderNoProtection(shader);
}

Material::Material() : shaderInUse(nullptr), shaderResource(nullptr)
{
    SetName("new material");
    shaderResource = GetGfxDriver()->CreateShaderResource();
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
    auto iter = textureValues.find(param);
    bool same = false;
    if (iter != textureValues.end())
    {
        same = iter->second == texture;
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
    SetDirty();
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
            mat->shaderResource->SetBuffer(PerMaterial, mat->ubo.buffer.get());
            mat->ubo.dirty = true;
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
    SetDirty();
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
        shaderFeatures = &ShaderLibrary::QueryShaderFeatures(shaderName.data());

        if (shaderFeatures)
        {
            auto perm = shaderFeatures->GetPermutation(enabledFeatures);
            auto shader = ShaderLibrary::GetShader(shaderName.data(), perm);
            needRequestNewShader = false;
            this->shaderName = shaderName;

            if (shader == nullptr)
                return;

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
    if (uploadNeeded && shaderInUse)
    {
        UploadDataToGPU(shaderInUse->GetShaderProgram());
    }
    return shaderResource.get();
}

Gfx::ShaderProgram* Material::GetShaderProgram()
{
    if (needRequestNewShader && shaderFeatures)
    {
        shaderInUse = ShaderLibrary::GetShader(shaderName.data(), shaderFeatures->GetPermutation(enabledFeatures));
        needRequestNewShader = false;
    }

    return shaderInUse->GetShaderProgram();
}

void Material::EnableFeature(const std::string& name)
{
    if (!enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.emplace(name);
    }
}

void Material::DisableFeature(const std::string& name)
{
    if (enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.erase(name);
    }
}

void Material::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    // s->Deserialize("shader", shader);
    s->Deserialize("ubo", ubo);
    s->Deserialize(
        "textureValues",
        textureValues,
        [this](void* res)
        {
            if (res)
            {
                Texture* tex = (Texture*)res;
                for (auto& kv : textureValues)
                {
                    if (kv.second == tex)
                    {
                        SetTextureInternal(kv.first, tex, std::nullopt);
                        break;
                    }
                }
            }
        }
    );
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
    return;
    uploadNeeded = true;

    bool hasConfig = overrideShaderConfig;
    using namespace Gfx;

    CullMode cullMode = CullMode::Back;
    Topology topology = Topology::TriangleList;

    if (hasConfig)
    {
        auto config = *shaderConfig;
        cullMode = config.cullMode;
        topology = config.topology;
    }

    this->SetShader(shaderName);

    if (hasConfig)
    {
        auto copy = *shaderConfig;
        copy.cullMode = cullMode;
        copy.topology = topology;
        *shaderConfig = copy;
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
        auto descriptorSet = pipelineInfo.GetDescriptorSet(PerMaterial);
        if (descriptorSet == nullptr)
            return;

        auto binding = descriptorSet->GetBinding(PerMaterial);
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

                shaderResource->SetBuffer(PerMaterial, buffer.get());
                ubo.buffer = std::move(buffer);
            }

            auto bufSize = binding->byteSize;
            std::vector<uint8_t, GlobalTempAllocator<uint8_t>> tempUploadData(bufSize);

            ASSERT(binding->descriptorType == Gfx::DescriptorType::UniformBuffer && "UBO should be a structure");
            for (auto& member : binding->bufferMembers)
            {
                UploadDataToGPUInternal(pipelineInfo, member, tempUploadData);
            }
            GetGfxDriver()->UploadBuffer(*ubo.buffer, tempUploadData.data(), tempUploadData.size(), 0);
        }
    }
}

void Material::UploadDataToGPUInternal(
    const Gfx::PipelineInfo& pipelineInfo,
    const Gfx::PipelineInfo::BufferMember& bufferDataDescription,
    std::vector<uint8_t, GlobalTempAllocator<uint8_t>>& buf
)
{
    size_t offset = bufferDataDescription.offset;
    size_t bufSize = buf.size();
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
                    *((glm::vec4*)(buf.data() + offset)) = iter->second;
                }
                else if (bufferDataDescription.rowCount == 2)
                {
                    ASSERT(offset + sizeof(glm::vec2) <= bufSize);
                    *((glm::vec2*)(buf.data() + offset)) = glm::vec2(iter->second);
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
                *((glm::mat4*)(buf.data() + offset)) = iter->second;
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
                        *((float*)(buf.data() + offset)) = iter->second;
                    }
                    break;
                }
            case Gfx::PipelineInfo::MemberDataType::UInt:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(uint32_t) <= bufSize);
                        *((uint32_t*)(buf.data() + offset)) = (uint32_t)iter->second;
                    }
                    break;
                }
            case Gfx::PipelineInfo::MemberDataType::Int:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(int32_t) <= bufSize);
                        *((int32_t*)(buf.data() + offset)) = (int32_t)iter->second;
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

int Material::GetSet(const std::string& name) const
{
    if (shaderInUse)
    {
        return shaderInUse->GetShaderProgram()->GetShaderInfo().GetDescriptorSet(name)->setNum;
    }
    return -1;
}
