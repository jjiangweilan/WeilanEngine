#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Modules/Ocean/OceanQuadTree.hpp"
#include "Modules/Ocean/OceanRenderer.hpp"
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

    std::unique_ptr<Component> Clone(GameObject& owner) override;

    struct OceanConfig
    {
        int mipLevels = 8;
        float resolution = 1.0f;
        std::vector<float> lodViewDistance = {25.0f, 300.0f, 500.0f, 800.0f, 1200.0f, 1600.0f, 2400.0f, 6400.0f};
        std::vector<int> lodMeshVertices = {257, 129, 65, 33, 17, 8, 5, 3};

        INLINE_DEFINE_SERIALIZABLE(
            SER(mipLevels),
            SER(resolution),
            SER(lodViewDistance),
            SER(lodMeshVertices)
        );
    };

    void Reset();
    void OnInit() override;
    void Render(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData) override;
    void RandomizeWaves(int iteration);
    float TestThis(float h) { return h + 5.0f; };
    void Copy(OceanComponent& src);

    std::vector<CPUWave>& GetWaves() { return waves; }
    CPUWave& GetGlobalTweak() { return globalTweak; }
    Material& GetMaterial() { return material; }
    float& GetAreaScale() { return areaScale; }
    OceanConfig& GetConfig() { return config; }
    const std::vector<GPUResources::Wave>& GetGPUWaveCache() { return gpuWaveCache; };

    void UpdateWaveBuffer();
    void TransformChanged() override;
    void OnLoaded() override;

private:
    OceanRenderer renderer;
    OceanQuadTree quadTree;

    std::unique_ptr<Mesh> plane;
    ObjPtr<Shader> oceanShader;

    Material material;
    float areaScale;
    OceanConfig config;

    std::vector<CPUWave> waves;
    std::vector<GPUResources::Wave> gpuWaveCache;
    CPUWave globalTweak;
    std::unique_ptr<Gfx::Buffer> waveBuffer;
};
