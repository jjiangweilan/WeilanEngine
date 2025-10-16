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

    globalTweak = {{0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, 1.0f};

    UpdateWaveBuffer();
}

void OceanComponent::UpdateWaveBuffer()
{
    if (waves.empty())
    {
        material.SetFloat("waveCount", 0.0f);
        return;
    }

    // Create or recreate wave buffer
    if (waveBuffer == nullptr)
    {
        waveBuffer = GetGfxDriver()->CreateBuffer(
            waves.size() * sizeof(Wave),
            Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst, false, true, "WaveBuffer"
        );
    }

    std::vector<Wave> upload = waves;
    // Apply global tweak on CPU side so GPU buffer matches shader usage
    for (auto& w : upload)
    {
        w.direction = glm::normalize(w.direction + globalTweak.direction);
        w.amplitude *= globalTweak.amplitude;
        w.wavelength *= globalTweak.wavelength;
        w.speed *= globalTweak.speed;
        w.steepness *= globalTweak.steepness;
    }

    // Upload wave data
    GetGfxDriver()->UploadBuffer(*waveBuffer, (uint8_t*)upload.data(), upload.size() * sizeof(Wave));

    // Bind to material
    material.SetBuffer("waves", waveBuffer.get());

    material.SetFloat("waveCount", (float)waves.size());
}

void OceanComponent::Render(Gfx::CommandBuffer& cmd)
{
    Rendering::DrawMesh(cmd, *plane, material, gameObject->GetWorldMatrix(), materialSet);
}
