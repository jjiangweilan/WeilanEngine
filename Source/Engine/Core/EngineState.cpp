#include "EngineState.hpp"

EngineState& EngineState::GetSingleton()
{
    static EngineState engineState = EngineState();
    return engineState;
}
