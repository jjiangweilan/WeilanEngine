#pragma once
#include "Core/Component/Component.hpp"
#include "Core/Math/Geometry.hpp"

class ReflectionProbe : public Component
{
    DECLARE_OBJECT();

public:
    enum class UpdateType
    {
        Local
    };

    ReflectionProbe();
    ReflectionProbe(GameObject* gameObject);
    ~ReflectionProbe();
    const std::string& GetName() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    std::span<Frustum> GetViewProjectionMatrices();
    const float4x4& GetViewMatrix(int faceIdx);
    const float4x4& GetProjectionMatrix();

    float GetNear();
    float GetFar();
    float GetProjectionTop();
    float GetProjectionRight();
    uint32_t GetResolution();

    // void Serialize(Serializer* s) const override;
    // void Deserialize(Serializer* s) override;
    void OnInit() override;
    void OnEnable() override;
    void OnDisable() override;

    void SetUpdateType(UpdateType type) { updateType = type; }
    auto GetUpdateType() { return updateType; }

    Frustum GetFrustum(int faceIndex)
    {
        if (faceIndex < 0 || faceIndex > 5)
        {
            return frustums[0];
        }

        return frustums[faceIndex];
    }

    Gfx::Image* GetCubemap();

private:
    const float near = 0.1f;
    const float far = 1000.f;
    float4x4 projectionMatrix;
    Frustum frustums[6];
    float4x4 viewMatrices[6];
    UpdateType updateType = UpdateType::Local;
    std::unique_ptr<Gfx::Image> cubemap;
    uint32_t resolution = 512;

    void TransformChanged() override;
    void UpdateFrustums(float3 position);
};
