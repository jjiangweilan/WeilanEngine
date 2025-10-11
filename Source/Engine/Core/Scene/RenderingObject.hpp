#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/RenderPipeline/RenderEvents.hpp"
#include <cinttypes>

class RenderingObjectBase
{
protected:
    Rendering::RenderEvents renderEvent = Rendering::RenderEvents::None;

public:
    RenderingObjectBase()
    {
        renderEvent = DefineRenderEvent();
    };

    virtual void Render(Gfx::CommandBuffer& cmd) {};
    Rendering::RenderEvents GetRenderEvent() { return renderEvent; }

protected:
    virtual Rendering::RenderEvents DefineRenderEvent() { return renderEvent; }
};

class RenderingObjectID
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
