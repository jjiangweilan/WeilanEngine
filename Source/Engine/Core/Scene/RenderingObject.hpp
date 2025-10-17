#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/RenderPipeline/RenderEvents.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <cinttypes>

class RenderingObjectBase
{
    Rendering::RenderEvents renderEvent = Rendering::RenderEvents::None;

public:
    RenderingObjectBase() {};

    virtual void Render(Gfx::CommandBuffer& cmd, const Rendering::RenderPipelineSetting& settings) {};
    Rendering::RenderEvents GetRenderEvent() { return renderEvent; }

protected:
    Rendering::RenderEvents SetRenderEvent(Rendering::RenderEvents renderEvent) { return this->renderEvent = renderEvent; }
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
