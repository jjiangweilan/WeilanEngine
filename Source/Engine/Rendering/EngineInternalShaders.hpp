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

    static Shader* GetTriangleShader()
    {
        return GetSingleton().triangleShader;
    }

    static Shader* GetJoltDebugShader()
    {
        return GetSingleton().joltDebugShader;
    }

private:
    EngineInternalShaders();
    Shader* lineShader;
    Shader* triangleShader;
    Shader* joltDebugShader;
    static EngineInternalShaders& GetSingleton();
};
