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
    StaticMeshInstanceProperty properties;

    const AABB& GetAABB() const { return aabb; };

private:
    AABB aabb;
};

class StaticMeshInstanceHandle
{
public:
    StaticMeshInstanceHandle(StaticMeshInstance* instance, RenderSceneImpl* renderSceneImpl)
        : instance(instance), renderSceneImpl(renderSceneImpl) {};

    void SetTransform(const float4x4& transform);

private:
    StaticMeshInstance* instance;
    RenderSceneImpl* renderSceneImpl;
};
