#pragma once
#include "Engine/Library/DynamicArray.hpp"

#define ENGINE_DEBUG_VAR(name)                                                                                         \
    static bool& name()                                                                                                \
    {                                                                                                                  \
        static bool val = false;                                                                                       \
        return val;                                                                                                    \
    }

class EngineDebugVars
{
public:
    ENGINE_DEBUG_VAR(SceneBVH);
};
