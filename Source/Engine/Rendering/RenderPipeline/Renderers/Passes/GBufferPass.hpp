#pragma once

#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Rendering/DrawList.hpp"
#include "Rendering/RenderingData.hpp"

namespace Rendering
{

struct GBufferPassSetting
{
    glm::vec4 clearValuesVal;
};

class GBufferPass
{
public:
    struct
    {
        Gfx::RG::ImageIdentifier color;
        Gfx::RG::ImageIdentifier depth;
        Gfx::RG::ImageDescription colorDesc;
    } input;

    struct
    {
        Gfx::RG::ImageIdentifier albedo = Gfx::RG::ImageIdentifier("gbuffer albedo");
        Gfx::RG::ImageIdentifier normal = Gfx::RG::ImageIdentifier("gbuffer normal");
        Gfx::RG::ImageIdentifier mask = Gfx::RG::ImageIdentifier("gbuffer property");
    } output;

    GBufferPass();
    void Execute(Gfx::CommandBuffer& cmd, Rendering::RenderingData& renderingData, GBufferPassSetting& setting);

private:
    const DrawList* drawList;

    Gfx::RG::ImageDescription albedoDesc;
    Gfx::RG::ImageDescription normalDesc;
    Gfx::RG::ImageDescription maskDesc;

    Gfx::RG::RenderPass gbufferPass;
    uint64_t alphaTestFeatureID;
};
} // namespace Rendering
