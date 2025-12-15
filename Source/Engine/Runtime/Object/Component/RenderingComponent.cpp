#include "RenderingComponent.hpp"
#include "Runtime/System/SceneManager/RenderingScene.hpp"
#include "Runtime/System/SceneManager/Scene.hpp"

void RenderingComponentBase::AddToRenderingScene(uint32_t objectTypeID, RenderingObjectBase* self)
{
    renderingObjectID = GetScene()->GetRenderingScene().AddRenderingObject(objectTypeID, self);
}

void RenderingComponentBase::RemoveFromRenderingScene(uint32_t objectTypeID)
{
    GetScene()->GetRenderingScene().RemoveRenderingObject(objectTypeID, renderingObjectID);
}
