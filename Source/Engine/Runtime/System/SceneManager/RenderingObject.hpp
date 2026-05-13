#pragma once
#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderEvents.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include <cinttypes>

class RenderingObjectBase
{
    Rendering::RenderEvents renderEvent = Rendering::RenderEvents::None;

public:
    RenderingObjectBase() {};

    virtual void Render(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData) {};
    Rendering::RenderEvents GetRenderEvent() { return renderEvent; }

protected:
    Rendering::RenderEvents SetRenderEvent(Rendering::RenderEvents renderEvent) { return this->renderEvent = renderEvent; }
};

class WEILAN_ENGINE_API RenderingObjectID
{
public:
    static uint32_t GenerateRenderingObjectTypeID();
};

template <class T>
class RenderingObject : public RenderingObjectBase
{
public:
    static const uint32_t renderObjectTypeID;

private:
};

template <class T>
const uint32_t RenderingObject<T>::renderObjectTypeID = RenderingObjectID::GenerateRenderingObjectTypeID();
