#include "Profiler.hpp"

Profiler& Profiler::GetSingleton()
{
    static Profiler profiler;
    return profiler;
}
