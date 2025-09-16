#pragma once
#include <cinttypes>

class RenderingObjectBase
{};

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
