#pragma once
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "bend_sss_cpu.hpp"

namespace Rendering
{
class ContactShadowPass
{
public:
    ContactShadowPass();

    const Gfx::RG::ImageIdentifier& GetOutputId() const { return outputId; }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, Light* mainLight, Gfx::Image* depthTex);

private:
    ObjPtr<Shader2> shader;
    Gfx::RG::ImageIdentifier outputId = "ContactShadow";
    Gfx::RG::ImageDescription desc;
    Material mat;

    Material* RequestMaterial(int idx);
};
} // namespace Rendering
