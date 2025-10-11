#include "OceanComponent.hpp"
#include "Libs/CppUtility.hpp"
DEFINE_RENDERING_COMPONENT(OceanComponent, "503C87A6-3892-4EA7-877C-01CAA53E73AB")

DEFINE_SERIALIZATION(
    OceanComponent,
    SER(plane)
);

void OceanComponent::OnInit()
{
    oceanShader = ShaderLibrary::GetShader(Shaders::Ocean);
}

void OceanComponent::Render(Gfx::CommandBuffer& cmd)
{
    spdlog::info("Hello");
}
