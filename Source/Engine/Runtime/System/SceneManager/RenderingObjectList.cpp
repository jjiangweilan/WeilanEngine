#include "RenderingObjectList.hpp"
#include <algorithm>

RenderingObjectList::RenderingObjectList() : renderingObjectsByEvent()
{
    while(renderingObjectsByEvent.size() < (int)Rendering::RenderEvents::MAX_COUNT)
    {
        renderingObjectsByEvent.push_back(
            std::unique_ptr<std::vector<RenderingObjectBase*>>{new std::vector<RenderingObjectBase*>()}
        );
    }
}

void RenderingObjectList::EnsureCapacity(uint32_t typeID)
{
    while (typeID >= renderingObjects.size())
    {
        renderingObjects.push_back(
            std::unique_ptr<std::vector<RenderingObjectBase*>>{new std::vector<RenderingObjectBase*>()}
        );
    }
}

void RenderingObjectList::AddToList(uint32_t objectTypeID, RenderingObjectBase* object)
{
    EnsureCapacity(objectTypeID);

    Rendering::RenderEvents renderEvent = object->GetRenderEvent();

    renderingObjects[objectTypeID]->push_back(object);
    if (renderEvent != Rendering::RenderEvents::None)
    {
        renderingObjectsByEvent[static_cast<int>(renderEvent)]->push_back(object);
    }

}

void RenderingObjectList::RemoveFromList(uint32_t objectTypeID, RenderingObjectBase* object)
{
    if (objectTypeID >= renderingObjects.size() || object == nullptr)
        return;

    auto removeObject = [object](std::vector<RenderingObjectBase*>& objects)
    {
        auto iter = std::find(objects.begin(), objects.end(), object);
        if (iter == objects.end())
            return;

        std::swap(*iter, objects.back());
        objects.pop_back();
    };

    removeObject(*renderingObjects[objectTypeID]);

    for (auto& eventObjects : renderingObjectsByEvent)
    {
        removeObject(*eventObjects);
    }
}

void RenderingObjectList::Clear()
{
    renderingObjects.clear();
    for (auto& objects : renderingObjectsByEvent)
    {
        objects->clear();
    }
}

RenderingObjectList::ObjectList RenderingObjectList::GetRenderingObjects(uint32_t typeID)
{
    if (typeID >= renderingObjects.size())
    {
        static std::vector<RenderingObjectBase*> empty{};
        return empty;
    }

    return *renderingObjects[typeID];
}

RenderingObjectList::ObjectList RenderingObjectList::GetRenderingObjectsByEvent(Rendering::RenderEvents event)
{
    if (event == Rendering::RenderEvents::None)
    {
        static std::vector<RenderingObjectBase*> empty{};
        return empty;
    }

    int eventIdx = (int)event;
    return *renderingObjectsByEvent[eventIdx];
}
