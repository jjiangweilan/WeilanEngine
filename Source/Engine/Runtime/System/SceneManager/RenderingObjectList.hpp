#pragma once

#include "RenderingObject.hpp"
#include <cinttypes>
#include <memory>
#include <span>
#include <vector>

class RenderingObjectList
{
    std::vector<std::unique_ptr<std::vector<RenderingObjectBase*>>> renderingObjects;
    std::vector<std::unique_ptr<std::vector<RenderingObjectBase*>>> renderingObjectsByEvent;

public:
    using ObjectIndex = uint32_t;
    using ObjectList = std::span<RenderingObjectBase*>;

    RenderingObjectList();
    ObjectIndex AddToList(uint32_t objectTypeID, RenderingObjectBase* object);
    void RemoveFromList(uint32_t objectTypeID, ObjectIndex object);
    ObjectList GetRenderingObjects(uint32_t typeID);
    ObjectList GetRenderingObjectsByEvent(Rendering::RenderEvents event);

private:
    void EnsureCapacity(uint32_t typeID);
};
