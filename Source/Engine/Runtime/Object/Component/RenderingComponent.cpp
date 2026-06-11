#include "RenderingComponent.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingScene.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"

void RenderingComponentBase::AddToRenderingScene(uint32_t objectTypeID, RenderingObjectBase* self)
{
    GetScene()->GetRenderingScene().AddRenderingObject(objectTypeID, self);
}

void RenderingComponentBase::RemoveFromRenderingScene(uint32_t objectTypeID, RenderingObjectBase* self)
{
    GetScene()->GetRenderingScene().RemoveRenderingObject(objectTypeID, self);
}
