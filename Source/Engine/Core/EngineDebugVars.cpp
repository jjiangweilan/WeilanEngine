#include "EngineDebugVars.hpp"

#define ENGINE_DEBUG_VAR(name)                                                                                         \
    bool& EngineDebugVars::name()                                                                                      \
    {                                                                                                                  \
        static bool val = false;                                                                                       \
        return val;                                                                                                    \
    }

ENGINE_DEBUG_VAR(SceneBVH);
