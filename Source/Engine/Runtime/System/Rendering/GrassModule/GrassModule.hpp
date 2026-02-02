#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include <vector>

namespace Rendering
{
class GrassModule
{
public:
    void AddGrass(float4 position);

    void BeforeRenderSceneUpdate();
    void Render(Gfx::CommandBuffer& cmd);

private:
    std::vector<float4> grassPositions = {};
    ObjPtr<Shader> grassShader = nullptr;
    ObjPtr<Mesh> grassCluser = nullptr;
};
} // namespace Rendering
