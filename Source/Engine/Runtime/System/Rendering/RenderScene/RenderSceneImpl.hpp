#pragma once
#include "../RenderCamera.hpp"
#include "../StaticMeshRenderer.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "RenderSceneBVH.hpp"
#include "StaticMeshInstance.hpp"
#include <memory>

class RenderSceneImpl
{
public:
    StaticMeshRenderer CreateStaticMeshRenderer()
    {
    }

    void DestroyStaticMeshRenderer(StaticMeshRenderer& renderer)
    {
    }

    void SetInstanceTransform(int instanceIndex, const float4x4& transform)
    {
        auto& staticMesh = renderInstances[instanceIndex];

        if (staticMesh)
        {
            staticMesh->SetTransform(transform);
            sceneBvh.bvh.AppendRefitObject(staticMesh.get());
        }
    }

    RenderCamera CreateRenderCamera()
    {
        return RenderCamera{};
    }

private:
    std::vector<std::unique_ptr<RenderInstance>> renderInstances;

    struct
    {
        RenderSceneBVH bvh;
        bool rebuild = true;
    } sceneBvh;
};
