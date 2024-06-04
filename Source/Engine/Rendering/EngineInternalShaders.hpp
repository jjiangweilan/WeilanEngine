#pragma once

#include "Asset/Shader.hpp"

class EngineInternalShaders
{
public:
    static void Init();

    static Shader* GetLineShader()
    {
        return GetSingleton().lineShader;
    }

private:
    EngineInternalShaders();
    Shader* lineShader;
    static EngineInternalShaders& GetSingleton();
};
