#include "RenderingObject.hpp"

uint32_t RenderingObjectID::GenerateRenderingObjectTypeID()
{
    static uint32_t idx = 0;

    return idx++;
}
