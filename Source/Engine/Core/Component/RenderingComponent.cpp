#include "RenderingComponent.hpp"
#include "Core/Scene/RenderingScene.hpp"
#include "Core/Scene/Scene.hpp"

void RenderingComponentBase::AddToRenderingScene(uint32_t objectTypeID, RenderingObjectBase* self)
{
    renderingObjectID = GetScene()->GetRenderingScene().AddRenderingObject(objectTypeID, self);
}

void RenderingComponentBase::RemoveFromRenderingScene(uint32_t objectTypeID)
{
    GetScene()->GetRenderingScene().RemoveRenderingObject(objectTypeID, renderingObjectID);
}
