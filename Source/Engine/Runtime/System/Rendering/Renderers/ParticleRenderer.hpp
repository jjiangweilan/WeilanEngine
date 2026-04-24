#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
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
