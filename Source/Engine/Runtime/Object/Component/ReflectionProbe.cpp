#include "ReflectionProbe.hpp"
#include "Runtime/System/SceneManager/Scene.hpp"
#include "Editor/Gizmos/ScaleBoxGizmo.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"

DEFINE_OBJECT(ReflectionProbe, "E93609AC-6D6C-4F13-9538-CD63E2D58567")

ReflectionProbe::ReflectionProbe() : RenderingComponent() {}
ReflectionProbe::ReflectionProbe(GameObject* go) : RenderingComponent(go) {}
ReflectionProbe::~ReflectionProbe() {}
const std::string& ReflectionProbe::GetName() const
{
    static std::string name = "ReflectionProbe";
    return name;
}

void ReflectionProbe::OnEnable()
{
    RenderingComponent::OnEnable();

    UpdateFrustums(GetGameObject()->GetPosition());
}

void ReflectionProbe::EnsureIBLProbe()
{
}

void ReflectionProbe::OnDisable()
{
    RenderingComponent::OnDisable();
}

void ReflectionProbe::OnInit()
{
    Gfx::ImageDescription desc(256, 256, 1, Gfx::GfxFormat::B10G11R11_UFloat_Pack32, Gfx::MultiSampling::Sample_Count_1, 6, true);
    cubemap = GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Storage);
    cubemap->SetName("Reflection Probe Cubemap");

    totalPixelCount = 0;
    for (int mip = 0; mip < desc.mipLevels; mip++)
    {
        int mipWidth = desc.width * glm::pow(0.5, mip);
        int mipHeight = desc.height * glm::pow(0.5, mip);

        int facePixelCount = mipWidth * mipHeight;
        int mipPixelCount = facePixelCount * 6;

        totalPixelCount += mipPixelCount;
    }

    roughness[0] = 0.01f;
    roughness[1] = 0.2f;
    roughness[2] = 0.4f;
    roughness[3] = 0.6f;
    roughness[4] = 0.8f;
    roughness[5] = 0.999f;

    desc.mipLevels = (int)glm::log2((float)desc.width) + 1;
    cubemapBase = GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Storage);
    cubemapBase->SetName("Reflection Probe Base Cubemap");
    cubemapBaseMat.SetShader(Shaders::ReflectionProbeSkybox);
    float width = cubemapBase->GetDescription().width;
    cubemapBaseMat.SetVector("resolution", float4(width, width, 1.0f / width, 1.0f / width));

    auto& imageView = cubemapBase->GetImageView(Gfx::ImageViewOption(0, 1, 0, Gfx::Remaining_Array_Layers, Gfx::ImageAspect::Color));

    cubemapBaseMat.GetShaderResource()->SetImage("outputCubemap", &imageView);
}

void ReflectionProbe::TransformChanged()
{
    UpdateFrustums(GetGameObject()->GetPosition());
}

void ReflectionProbe::UpdateFrustums(float3 position)
{
    projectionMatrix = Math::PerspectiveProjectionMatrix(glm::radians(90.0f), 1, GetNear(), GetFar());
    float3 faces[] = {
        {1, 0, 0},  // +X
        {-1, 0, 0}, // -X
        {0, 1, 0},  // +Y
        {0, -1, 0}, // -Y
        {0, 0, 1},  // +Z
        {0, 0, -1}  // -Z
    };
    for (int i = 0; i < 6; ++i)
    {
        viewMatrices[i] =
            glm::lookAtRH(position, position + faces[i], (i != 2 && i != 3) ? float3(0, 1, 0) : float3(1, 0, 0));

        frustums[i] = projectionMatrix * viewMatrices[i];
    }
}

Gfx::Image* ReflectionProbe::GetCubemap()
{
    return cubemap.get();
}

const float4x4& ReflectionProbe::GetViewMatrix(int faceIdx)
{
    return viewMatrices[faceIdx];
}

const float4x4& ReflectionProbe::GetProjectionMatrix()
{
    return projectionMatrix;
}

float ReflectionProbe::GetNear()
{
    return near;
}

float ReflectionProbe::GetFar()
{
    return far;
}

float ReflectionProbe::GetProjectionTop()
{
    return near / projectionMatrix[0][0];
}

float ReflectionProbe::GetProjectionRight()
{
    return -near / projectionMatrix[1][1];
}

uint32_t ReflectionProbe::GetResolution()
{
    return resolution;
}

void ReflectionProbe::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, updateType);
    SERIALIZE(s, sourceType);
}

void ReflectionProbe::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    int iUpdateType;
    int iSourceType;
    DESERIALIZE(s, iSourceType);
    DESERIALIZE(s, iUpdateType);
    updateType = static_cast<ProbeType>(iUpdateType);
    sourceType = static_cast<SourceType>(iSourceType);
}

void ReflectionProbe::SetFar(float far)
{
    this->far = far;
    UpdateFrustums(gameObject->GetPosition());
}
void ReflectionProbe::SetNear(float near)
{
    this->near = near;
    UpdateFrustums(gameObject->GetPosition());
}

void ReflectionProbe::BakeStaticReflectionProbe()
{
    for (int face = 0; face < 6; face++)
    {
        std::unique_ptr<Rendering::RenderPipeline> renderPipeline = std::make_unique<Rendering::RenderPipeline>();
    }
}

void ReflectionProbe::OnDrawGizmos()
{
    Gizmos::DrawInteractiveBox(gizmoState, GetGameObject()->GetPosition(), extent);
}

int ReflectionProbe::GetTotalPixelCount()
{
    return totalPixelCount;
}

void ReflectionProbe::OnDrawGizmos(GizmoManager& gizmoContext) {}

Gfx::Image* ReflectionProbe::GetCubemapBase()
{
    return cubemapBase.get();
}

Material& ReflectionProbe::GetCubemapBaseMaterial()
{
    return cubemapBaseMat;
}

Gfx::ShaderResource* ReflectionProbe::EnsureAndGetShaderResource(std::vector<std::unique_ptr<Gfx::ImageView>>& cubemapImageViews)
{
    if (probeUpdateShaderResource != nullptr)
    {
        return probeUpdateShaderResource.get();
    }

    probeUpdateShaderResource = GetGfxDriver()->CreateShaderResource();
    probeUpdateShaderResource->SetBuffer("input", &*shaderInput);
    probeUpdateShaderResource->SetImage("srcCubemap", GetCubemapBase());

    shaderInput->envMapSize = GetCubemap()->GetDescription().width;
    shaderInput->envMapSizeSqr = shaderInput->envMapSize * shaderInput->envMapSize;
    shaderInput->totalPixelCount = GetTotalPixelCount();
    shaderInput->roughness[0] = 0.0001;
    shaderInput->roughness[1] = 0.2;
    shaderInput->roughness[2] = 0.4;
    shaderInput->roughness[3] = 0.6;
    shaderInput->roughness[4] = 0.8;
    shaderInput->roughness[5] = 0.9999;

    GetGfxDriver()->UploadBuffer(*shaderInput, (uint8_t*)shaderInput.GetPtr(), shaderInput.GetSize());

    for (int i = 0; i < 36; ++i)
    {
        probeUpdateShaderResource->SetImage(Gfx::ShaderBindingHandle("dstFaces"), i, cubemapImageViews[i].get());
    }

    return probeUpdateShaderResource.get();
}
