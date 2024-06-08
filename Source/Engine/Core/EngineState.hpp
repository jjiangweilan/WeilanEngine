#pragma once

class EngineState
{
public:
    static EngineState& GetSingleton();

    bool isPlaying;
};
