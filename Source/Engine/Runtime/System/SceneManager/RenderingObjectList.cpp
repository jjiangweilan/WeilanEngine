#include "RenderingObjectList.hpp"

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

RenderingObjectList::ObjectIndex RenderingObjectList::AddToList(uint32_t objectTypeID, RenderingObjectBase* object)
{
    EnsureCapacity(objectTypeID);

    uint32_t idx = renderingObjects[objectTypeID]->size();
    Rendering::RenderEvents renderEvent = object->GetRenderEvent();

    renderingObjects[objectTypeID]->push_back(object);
    if (renderEvent != Rendering::RenderEvents::None)
    {
        renderingObjectsByEvent[static_cast<int>(renderEvent)]->push_back(object);
    }

    return idx;
}

void RenderingObjectList::RemoveFromList(uint32_t objectTypeID, ObjectIndex object)
{
    // no sanity check here, just trust the input

    RenderingObjectBase* back = renderingObjects[objectTypeID]->at(object);
    Rendering::RenderEvents renderEvent = back->GetRenderEvent();

    std::swap(renderingObjects[objectTypeID]->back(), renderingObjects[objectTypeID]->at(object));
    renderingObjects[objectTypeID]->pop_back();

    if (renderEvent != Rendering::RenderEvents::None)
    {
        std::swap(renderingObjectsByEvent[static_cast<int>(renderEvent)]->back(), renderingObjectsByEvent[static_cast<int>(renderEvent)]->at(object));
        renderingObjectsByEvent[static_cast<int>(renderEvent)]->pop_back();
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
