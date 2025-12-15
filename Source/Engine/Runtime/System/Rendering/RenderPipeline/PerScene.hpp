#pragma once

#include "Runtime/System/Rendering/RenderingData.hpp"

namespace Rendering
{
struct PerScene
{
    PerScene();
    GPUParameter::Camera cameraParameter{};
    GPUParameter::Scene sceneParameter{};
    GPUParameter::MainLightShadow mainLightShadowParameter{};

    std::unique_ptr<Gfx::ShaderResource> globalResource{};

    std::unique_ptr<Gfx::Buffer> scene{};
    std::unique_ptr<Gfx::Buffer> camera{};
    std::unique_ptr<Gfx::Buffer> mainLightShadow{};
};
} // namespace Rendering
