#pragma once
#include "Runtime/Object/Component/Camera.hpp"
#include "Runtime/Object/Component/Light.hpp"
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Driver/GfxDriver/RenderGraph.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"
#include "Runtime/System/Rendering/ShaderLibrary.hpp"
#include "bend_sss_cpu.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"

namespace Rendering
{
class ContactShadowPass : public RenderPipelinePass
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
