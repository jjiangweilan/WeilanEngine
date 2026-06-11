#pragma once

#include "Engine/WeilanEngineAPI.hpp"
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
    using ObjectList = std::span<RenderingObjectBase*>;

    WEILAN_ENGINE_API RenderingObjectList();
    WEILAN_ENGINE_API void AddToList(uint32_t objectTypeID, RenderingObjectBase* object);
    WEILAN_ENGINE_API void RemoveFromList(uint32_t objectTypeID, RenderingObjectBase* object);
    WEILAN_ENGINE_API void Clear();
    WEILAN_ENGINE_API ObjectList GetRenderingObjects(uint32_t typeID);
    WEILAN_ENGINE_API ObjectList GetRenderingObjectsByEvent(Rendering::RenderEvents event);

private:
    void EnsureCapacity(uint32_t typeID);
};
