#include "DebugOptions.hpp"


DebugOptions& GetDebugOptions()
{
    static DebugOptions options;
    return options;
}
