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
    DECLARE_RENDERING_COMPONENT(OceanComponent);

    struct CPUWave : public GPUResources::Wave
    {
        float directionAngle;
    };

    std::unique_ptr<Mesh> plane;
    ObjPtr<Shader> oceanShader;

    Material material;
    int materialSet;

    std::vector<CPUWave> waves;
    GPUResources::Wave globalTweak;
    std::unique_ptr<Gfx::Buffer> waveBuffer;

public:
    void OnInit() override;
    void Render(Gfx::CommandBuffer& cmd) override;

    std::vector<CPUWave>& GetWaves() { return waves; }
    GPUResources::Wave& GetGlobalTweak() { return globalTweak; }

    void UpdateWaveBuffer();

private:
};
