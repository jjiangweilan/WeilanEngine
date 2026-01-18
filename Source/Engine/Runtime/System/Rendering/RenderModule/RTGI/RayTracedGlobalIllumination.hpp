#pragma once
#include "../RenderModule.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

class RTGIContext
{
public:
    void SetTriangleBufferList();

private:
};

class RayTracedGlobalIllumination : public RenderModule
{
public:
    RayTracedGlobalIllumination();
    ~RayTracedGlobalIllumination();

    RTGIContext* GetRTGIContext();

    void Initialize();
    void Execute();
};
