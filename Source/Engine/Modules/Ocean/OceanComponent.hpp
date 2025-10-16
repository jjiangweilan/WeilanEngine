#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Rendering/Shader.hpp"

#include "Shaders/OceanInput.hlsl"

class OceanComponent : public RenderingComponent<OceanComponent>
{
    DECLARE_RENDERING_COMPONENT(OceanComponent);

    std::unique_ptr<Mesh> plane;
    ObjPtr<Shader> oceanShader;

    Material material;
    int materialSet;

    std::vector<Wave> waves;
    Wave globalTweak;
    std::unique_ptr<Gfx::Buffer> waveBuffer;

public:
    void OnInit() override;
    void Render(Gfx::CommandBuffer& cmd) override;

    std::vector<Wave>& GetWaves() { return waves; }
    Wave& GetGlobalTweak() { return globalTweak; }
    void UpdateWaveBuffer();

private:
};
