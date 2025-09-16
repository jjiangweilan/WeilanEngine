#pragma once

#include "RenderingObject.hpp"
#include <cinttypes>
#include <memory>
#include <span>
#include <vector>

class RenderingObjectList
{
public:
    using ObjectIndex = uint32_t;
    using ObjectList = std::span<RenderingObjectBase*>;

    ObjectIndex AddToList(uint32_t objectTypeID, RenderingObjectBase* object);
    void RemoveFromList(uint32_t objectTypeID, ObjectIndex object);

    ObjectList GetRenderingObjects(uint32_t typeID) { return *renderingObjects[typeID]; }

private:
    std::vector<std::unique_ptr<std::vector<RenderingObjectBase*>>> renderingObjects;

    void EnsureCapacity(uint32_t typeID);
};
