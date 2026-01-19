#pragma once
#include "Engine/Library/Math.hpp"
#include "RenderInstance.hpp"

class RenderSceneImpl;

struct StaticMeshInstanceProperty
{
    float4x4 transform = glm::float4x4(1.0f);
};

struct StaticMeshInstance : public RenderInstance
{
public:
    const AABB& GetAABB() const { return aabb; }

    void SetTransform(const float4x4& transform)
    {
        properties.transform = transform;
        UpdateAABB();
    }

private:
    StaticMeshInstanceProperty properties;
    AABB aabb;

    void UpdateAABB()
    {
    }
};
