#include "GrassSurface.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
DEFINE_RENDERING_COMPONENT(GrassSurface, "8B141EA8-BD84-4800-91AA-B07FCA7C7605")

void GrassSurface::Tick()
{
}

void GrassSurface::Serialize(Serializer* ser) const
{
    RenderingComponent::Serialize(ser);
    SERIALIZE(ser, grassPatchGroup);
}

void GrassSurface::Deserialize(Serializer* ser)
{
    RenderingComponent::Deserialize(ser);
    DESERIALIZE(ser, grassPatchGroup);
}

void GrassSurface::OnDrawGizmos(GizmoManager& manager)
{
}
