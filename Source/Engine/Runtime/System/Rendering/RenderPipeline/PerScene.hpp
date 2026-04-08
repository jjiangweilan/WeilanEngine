#pragma once

#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

namespace Rendering
{
struct PerScene
{
    PerScene();
    GPUParameter::Camera cameraParameter{};
    GPUParameter::Scene sceneParameter{};
    GPUParameter::MainLightShadow mainLightShadowParameter{};

    // Camera/scene/shadow buffers are owned here but bound into GPUDrivenManager's global descriptor set.
    Gfx::Buffer* scene{};
    Gfx::Buffer* camera{};
    Gfx::Buffer* mainLightShadow{};

    // The global descriptor set is owned by GPUDrivenManager; this is a cached pointer.
    Gfx::ShaderResource* GetGlobalResource() const;
};
} // namespace Rendering
