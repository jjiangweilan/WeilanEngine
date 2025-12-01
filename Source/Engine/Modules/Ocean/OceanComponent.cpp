#include "OceanComponent.hpp"
#include "Core/GameObject.hpp"
#include "Libs/CppUtility.hpp"
#include "Rendering/CommandBufferUtils.hpp"
#include "Rendering/GeometryUtils.hpp"
#include "Rendering/RenderPipeline/PerScene.hpp"
DEFINE_RENDERING_COMPONENT_CONSTRUCT(OceanComponent, "503C87A6-3892-4EA7-877C-01CAA53E73AB")
{
    // Initialize default waves
    waves.resize(4);
    waves[0] = {{{0.0f, 0.0f}, 0.4f, 8.0f, 1.0f, 0.6f}, true, false, 0.0f};
    waves[1] = {{{0.0f, 0.0f}, 0.3f, 6.0f, 1.2f, 0.5f}, true, false, 45.0f};
    waves[2] = {{{0.0f, 0.0f}, 0.2f, 4.0f, 1.5f, 0.4f}, true, false, 135.0f};
    waves[3] = {{{0.0f, 0.0f}, 0.15f, 3.0f, 1.8f, 0.3f}, true, false, 225.0f};

    globalTweak = {{{0.0f, 0.0f}, 1.0f, 1.0f, 1.0f}, true, false, 1.0f};
}

DEFINE_SERIALIZATION(
    OceanComponent,
    Component,
    SER(waves),
    SER(globalTweak),
    SER(material),
    SER(areaScale),
    SER(config)
)

void OceanComponent::OnInit()
{
    SetRenderEvent(Rendering::RenderEvents::ForwardOpaque);
    plane = Rendering::GeneratePlane(1, 1, 256, 256);
    oceanShader = ShaderLibrary::GetShader(Shaders::OceanPatchShader);
    material.SetShader(oceanShader);

    UpdateWaveBuffer();

    Reset();
}

void OceanComponent::Reset()
{
    OceanQuadTreeConfig quadTreeConfig;
    quadTreeConfig.resolution = config.resolution;
    quadTreeConfig.mipLevels = config.mipLevels;
    quadTreeConfig.lodViewDistance = config.lodViewDistance;

    quadTree.SetLODLevels(quadTreeConfig);
    renderer.Setup(config.lodMeshVertices);
}

void OceanComponent::RandomizeWaves(int iteration)
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + iteration);
    
    for (auto& wave : waves)
    {
        if (!wave.enabled)
            continue;
            
        // Randomize direction angle (0-360 degrees)
        wave.directionAngle = static_cast<float>(std::rand() % 360);
        
        // Randomize amplitude (0.1 to 0.5)
        wave.amplitude = 0.1f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.4f;
        
        // Randomize wavelength (2.0 to 10.0)
        wave.wavelength = 2.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 8.0f;
        
        // Randomize speed (0.5 to 2.0)
        wave.speed = 0.5f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.5f;
        
        // Randomize steepness (0.2 to 0.8)
        wave.steepness = 0.2f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.6f;
    }
    
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

    waveBuffer = GetGfxDriver()->CreateBuffer(
        waves.size() * sizeof(GPUResources::Wave),
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        true,
        "WaveBuffer"
    );

    std::vector<GPUResources::Wave>& upload = gpuWaveCache;
    upload.clear();
    upload.reserve(waves.size());

    // Convert CPUWave to Wave
    for (const auto& cpuWave : waves)
    {
        if (!cpuWave.enabled)
            continue;

        GPUResources::Wave gpuWave;

        // Convert angle (in degrees) to direction vector
        float angleRad = glm::radians(cpuWave.directionAngle);
        gpuWave.direction = glm::normalize(glm::vec2(glm::cos(angleRad), glm::sin(angleRad)));

        // Apply global tweak
        gpuWave.direction = glm::normalize(gpuWave.direction + globalTweak.direction);
        gpuWave.amplitude = cpuWave.amplitude * globalTweak.amplitude;
        gpuWave.wavelength = cpuWave.wavelength * globalTweak.wavelength;
        gpuWave.speed = cpuWave.speed * globalTweak.speed;
        gpuWave.steepness = cpuWave.steepness * globalTweak.steepness;

        if (globalTweak.clampCrest)
        {
            gpuWave.steepness = glm::min(gpuWave.steepness, gpuWave.wavelength / (gpuWave.amplitude));
        }

        upload.push_back(gpuWave);
    }

    // Upload wave data
    GetGfxDriver()->UploadBuffer(*waveBuffer, (uint8_t*)upload.data(), upload.size() * sizeof(GPUResources::Wave));

    material.SetBuffer("waves", waveBuffer.get());
    material.SetFloat("areaScale", areaScale);
    material.SetFloat("waveCount", (float)upload.size());
    material.SetFloat("yOffset", GetGameObject()->GetPosition().y);
}

void OceanComponent::Render(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData)
{
    quadTree.UpdateQuadTree(renderingData.perScene->cameraParameter.position, renderingData.cameraFrustum);
    renderer.Render(cmd, quadTree, material, renderingData);
}
