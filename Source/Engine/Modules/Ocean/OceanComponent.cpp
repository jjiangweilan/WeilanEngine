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

    // Initialize default waves
    waves = {
        {{1.0f, 0.3f}, 0.4f, 8.0f, 1.0f, 0.6f},
        {{0.2f, 1.0f}, 0.3f, 6.0f, 1.2f, 0.5f},
        {{-0.7f, 0.5f}, 0.2f, 4.0f, 1.5f, 0.4f},
        {{-0.3f, -0.8f}, 0.15f, 3.0f, 1.8f, 0.3f}
    };

    UpdateWaveBuffer();
}

void OceanComponent::UpdateWaveBuffer()
{
    if (waves.empty())
        return;

    // Create or recreate wave buffer
    if (waveBuffer = nullptr)
    {
        waveBuffer = GetGfxDriver()->CreateBuffer(
            waves.size() * sizeof(Wave),
            Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst, false, true, "WaveBuffer"
        );
    }

    // Upload wave data
    GetGfxDriver()->UploadBuffer(*waveBuffer, (uint8_t*)waves.data(), waves.size() * sizeof(Wave));

    // Bind to material
    material.SetBuffer("waves", waveBuffer.get());
}

void OceanComponent::Render(Gfx::CommandBuffer& cmd)
{
    Rendering::DrawMesh(cmd, *plane, material, gameObject->GetWorldMatrix(), materialSet);
}
