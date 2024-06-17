#pragma once
#include "GfxDriver/GfxDriver.hpp"
namespace Rendering::LFP
{
struct Probe
{
    glm::vec3 position;

    // octahedral maps
    std::unique_ptr<Gfx::Image> albedo;
    std::unique_ptr<Gfx::Image> normal;
    std::unique_ptr<Gfx::Image> radialDistance;
};
} // namespace Rendering::LFP
