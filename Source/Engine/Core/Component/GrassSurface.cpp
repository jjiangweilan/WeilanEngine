#include "GrassSurface.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/Shader.hpp"

DEFINE_OBJECT(GrassSurface, "8B141EA8-BD84-4800-91AA-B07FCA7C7605")

GrassSurface::GrassSurface() : Component(nullptr)
{
    Init();
}
GrassSurface::GrassSurface(GameObject* gameObject) : Component(gameObject)
{
    Init();
}
GrassSurface::~GrassSurface() {}

std::unique_ptr<Component> GrassSurface::Clone(GameObject& owner)
{
    return nullptr;
}
const std::string& GrassSurface::GetName()
{
    static std::string name = "GrassSurface";
    return name;
}

void GrassSurface::Awake() {}

void GrassSurface::EnableImple()
{
    GetScene()->GetRenderingScene().AddGrassSurface(*this);
}
void GrassSurface::DisableImple()
{
    GetScene()->GetRenderingScene().RemoveGrassSurface(*this);
}

void GrassSurface::Init()
{
    Shader* grassShader =
        (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/Grass/Grass.shad");
    drawMat.SetShader(grassShader);

    ComputeShader* dispatchShader =
        (ComputeShader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/Grass/GrassDispatcher.comp"
        );
    computeDispatchMat.SetShader(dispatchShader);

    grassDispatcherIndirectDrawBuffer =
        GetGfxDriver()
            ->CreateBuffer(5 * sizeof(uint32_t), Gfx::BufferUsage::Storage | Gfx::BufferUsage::Indirect, false, true);

    computeDispatchMat.SetBuffer("GrassDispatchArgsBuf", grassDispatcherIndirectDrawBuffer.get());
}
