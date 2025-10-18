#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Rendering/Shader.hpp"

namespace GPUResources
{
#include "Shaders/OceanInput.hlsl"
}

class OceanComponent : public RenderingComponent<OceanComponent>
{
    DECLARE_RENDERING_COMPONENT_CONSTRUCT(OceanComponent);
    DECLARE_SERIALIZATION();

    struct CPUWave : public GPUResources::Wave
    {
        bool enabled;
        float directionAngle;

        INLINE_DEFINE_SERIALIZABLE(
            SER(enabled),
            SER(directionAngle),
            SER(amplitude),
            SER(wavelength),
            SER(speed),
            SER(steepness)
        );
    };

    std::unique_ptr<Mesh> plane;
    ObjPtr<Shader> oceanShader;

    Material material;
    int materialSet;

    std::vector<CPUWave> waves;
    CPUWave globalTweak;
    std::unique_ptr<Gfx::Buffer> waveBuffer;

public:
    void OnInit() override;
    void Render(Gfx::CommandBuffer& cmd, const Rendering::RenderPipelineSetting& settings) override;

    std::vector<CPUWave>& GetWaves() { return waves; }
    CPUWave& GetGlobalTweak() { return globalTweak; }

    void UpdateWaveBuffer();

private:
};
