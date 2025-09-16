#include "RenderingObjectList.hpp"

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
    renderingObjects[objectTypeID]->push_back(object);

    return idx;
}

void RenderingObjectList::RemoveFromList(uint32_t objectTypeID, ObjectIndex object)
{
    // no sanity check here, just trust the input

    std::swap(renderingObjects[objectTypeID]->back(), renderingObjects[objectTypeID]->at(object));

    renderingObjects[objectTypeID]->pop_back();
}
