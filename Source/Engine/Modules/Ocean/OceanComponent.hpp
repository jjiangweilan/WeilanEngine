#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Modules/Ocean/OceanQuadTree.hpp"
#include "Rendering/Shader.hpp"

namespace GPUResources
{
#include "Shaders/OceanInput.hlsl"
}

class OceanComponent : public RenderingComponent<OceanComponent>
{
    DECLARE_RENDERING_COMPONENT_CONSTRUCT(OceanComponent);
    DECLARE_SERIALIZATION();

public:
    struct CPUWave : public GPUResources::Wave
    {
        bool enabled;
        bool clampCrest;
        float directionAngle;

        INLINE_DEFINE_SERIALIZABLE(
            SER(enabled),
            SER(clampCrest),
            SER(directionAngle),
            SER(amplitude),
            SER(wavelength),
            SER(speed),
            SER(steepness)
        );
    };

    void OnInit() override;
    void Render(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData) override;

    std::vector<CPUWave>& GetWaves() { return waves; }
    CPUWave& GetGlobalTweak() { return globalTweak; }
    Material& GetMaterial() { return material; }
    float& GetAreaScale() { return areaScale; }
    const std::vector<GPUResources::Wave>& GetGPUWaveCache() { return gpuWaveCache; };

    void UpdateWaveBuffer();

private:
    std::unique_ptr<Mesh> plane;
    ObjPtr<Shader> oceanShader;

    Material material;
    int materialSet;
    float areaScale;

    OceanQuadTree quadTree;
    std::vector<CPUWave> waves;
    std::vector<GPUResources::Wave> gpuWaveCache;
    CPUWave globalTweak;
    std::unique_ptr<Gfx::Buffer> waveBuffer;
};
