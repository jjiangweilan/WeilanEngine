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

    Gfx::ImageIdentifier& GetOutputId();

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, Light* mainLight, Gfx::Image* depthTex);

private:
    ObjPtr<Shader> shader;
    Gfx::ImageIdentifier outputID;
    Gfx::ImageIdentifier contactShadowMap = "ContactShadow";
    Gfx::RenderImageDescriptor desc;
    Material mat;
    bool valid = false;

    Material* RequestMaterial(int idx);
};
} // namespace Rendering
