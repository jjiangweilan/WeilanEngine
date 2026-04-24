#pragma once
#include "Engine/Runtime/Object/Component/RenderingComponent.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingObject.hpp"
#include "Engine/Runtime/System/Rendering/GPUBuffer.hpp"
namespace GPUResources::ReflectionProbe
{
#include "Engine/Shaders/ReflectionProbeIBLGeneratorInput.hlsl"
};

class ReflectionProbe : public RenderingComponent<ReflectionProbe>
{
    DECLARE_OBJECT();

public:
    enum class SourceType
    {
        Static,
        Runtime
    };

    enum class ProbeType
    {
        Local,
        Global,
        SkyboxOnly
    };

private:
    float near = 0.1f;
    float far = 1000.f;
    float4x4 projectionMatrix;
    Frustum frustums[6];
    float4x4 viewMatrices[6];
    ProbeType updateType = ProbeType::Local;
    SourceType sourceType = SourceType::Static;
    /**
     * @brief extent of local reflection probe cube
     */
    float3 extent = {1, 1, 1};
    std::unique_ptr<Gfx::Image> cubemap;
    std::unique_ptr<Gfx::Image> cubemapBase;
    float roughness[6];
    uint32_t resolution = 512;
    ObjPtr<Texture> staticReflectionProbe;
    InteractiveBox gizmoState{};
    int totalPixelCount;
    Material cubemapBaseMat;
    std::unique_ptr<Gfx::ShaderResource> probeUpdateShaderResource;
    GPUBuffer<GPUResources::ReflectionProbe::ParameterInput> shaderInput = GPUBuffer<GPUResources::ReflectionProbe::ParameterInput>(true);

public:
    ReflectionProbe();
    ReflectionProbe(GameObject* gameObject);
    ~ReflectionProbe();
    const std::string& GetName() const override;

    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    std::span<Frustum> GetViewProjectionMatrices();
    const float4x4& GetViewMatrix(int faceIdx);
    const float4x4& GetProjectionMatrix();
    int GetTotalPixelCount();

    float GetNear();
    float GetFar();
    void SetFar(float far);
    void SetNear(float near);

    float GetProjectionTop();
    float GetProjectionRight();
    uint32_t GetResolution();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnInit() override;
    void OnEnable() override;
    void OnDisable() override;

    void EnsureIBLProbe();
    void BakeStaticReflectionProbe();
    void SetUpdateType(ProbeType type) { updateType = type; }
    auto GetUpdateType() { return updateType; }
    Gfx::ShaderResource* EnsureAndGetShaderResource(std::vector<std::unique_ptr<Gfx::ImageView>>& cubemapImageViews);
    

    void SetSourceType(SourceType type) { sourceType = type; }
    auto GetSourceType() { return sourceType; }

    void SetExtent(const float3& newExtent) { extent = newExtent; }
    const float3& GetExtent() const { return extent; }

    uint32_t GetResolution() const { return resolution; }

    Frustum GetFrustum(int faceIndex)
    {
        if (faceIndex < 0 || faceIndex > 5)
        {
            return frustums[0];
        }

        return frustums[faceIndex];
    }

    Gfx::Image* GetCubemap();
    Gfx::Image* GetCubemapBase();
    Material& GetCubemapBaseMaterial();
    void OnDrawGizmos() override;
    void OnDrawGizmos(GizmoManager& gizmoContext) override;

private:
    void TransformChanged() override;
    void UpdateFrustums(float3 position);
    void InitializeForSkyboxOnly();
};
