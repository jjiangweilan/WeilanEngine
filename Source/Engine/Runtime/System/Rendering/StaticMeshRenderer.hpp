#pragma once
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/System/Rendering/RenderCore/RenderCoreData.hpp"

class StaticMeshInstance;
class RenderSceneImpl;
class StaticMeshRenderer
{
public:
    StaticMeshRenderer(int instanceIndex, RenderSceneImpl* renderSceneImpl)
        : instanceIndex(instanceIndex), renderSceneImpl(renderSceneImpl) {};

    void SetTransform(const float4x4& transform);
    void SetMesh(MeshHandle mesh);

    int GetInstanceIndex() const { return instanceIndex; }

private:
    int instanceIndex = -1;
    RenderSceneImpl* renderSceneImpl;
};
