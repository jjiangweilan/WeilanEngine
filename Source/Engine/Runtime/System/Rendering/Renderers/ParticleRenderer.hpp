#pragma once
#include "Driver/GfxDriver/Buffer.hpp"
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Library/Math.hpp"
#include "Runtime/System/Rendering/Shader.hpp"
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
    ObjPtr<Shader> particleShader;
    int parameterSetIdx;
};
} // namespace Rendering
