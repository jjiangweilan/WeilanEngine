#pragma once
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Libs/Math.hpp"
#include "Rendering/Shader2.hpp"
#include <span>

class Submesh;

namespace Rendering
{
struct ParticleDraw
{
    Submesh* instancingMesh;
    size_t particleCount;
    Gfx::ShaderResource* particleShaderParameters;
    float4x4 rootTransform; 
};

class ParticleRenderer
{
public:
    ParticleRenderer();
    void Draw(Gfx::CommandBuffer& cmd, const ParticleDraw& draw);

private:
    ObjPtr<Shader2> particleShader;
    int parameterSetIdx;
};
} // namespace Rendering
