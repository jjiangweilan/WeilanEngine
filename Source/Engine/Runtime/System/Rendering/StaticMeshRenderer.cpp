#pragma once
#include "StaticMeshRenderer.hpp"
#include "RenderScene/RenderSceneImpl.hpp"

void StaticMeshRenderer::SetTransform(const float4x4& transform)
{
    renderSceneImpl->SetInstanceTransform(instanceIndex, transform);
}
