#include "OceanComponent.hpp"
#include "Libs/For_Each_Argument.hpp"
DEFINE_RENDERING_COMPONENT(OceanComponent, "503C87A6-3892-4EA7-877C-01CAA53E73AB")

void OceanComponent::OnInit()
{
    oceanShader = ShaderLibrary::GetShader(Shaders::Ocean);
}

DEFINE_SERIALIZATION(OceanComponent, SER(plane));
