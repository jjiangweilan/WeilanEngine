#include "OceanComponent.hpp"
#include "Core/GameObject.hpp"
#include "Libs/CppUtility.hpp"
#include "Rendering/CommandBufferUtils.hpp"
#include "Rendering/GeometryUtils.hpp"
DEFINE_RENDERING_COMPONENT(OceanComponent, "503C87A6-3892-4EA7-877C-01CAA53E73AB")

void OceanComponent::OnInit()
{
    SetRenderEvent(Rendering::RenderEvents::ForwardOpaque);

    plane = Rendering::GeneratePlane(5, 5, 64, 64);
    oceanShader = ShaderLibrary::GetShader(Shaders::Ocean);
    materialSet = oceanShader->GetSet(Gfx::DescriptorSetSemantics::Material);
    material.SetShader(oceanShader);
}

void OceanComponent::Render(Gfx::CommandBuffer& cmd)
{
    Rendering::DrawMesh(cmd, *plane, material, gameObject->GetWorldMatrix(), materialSet);
}
