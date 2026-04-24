#pragma once
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/FogPassParameters.hpp"

struct [[SerClass]] SceneEnvironmentData
{
    FogPassParameters fogPassParameters{};
};
