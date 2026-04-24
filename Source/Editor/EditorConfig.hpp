#pragma once
#include "Engine/Library/MathJson.hpp"
#include <fstream>

class EditorConfig
{
public:
    static EditorConfig& GetInstance();

    float4 GetSceneTreeGameObjectColor()
    {
        return j["sceneTree"]["gameObjectColor"].get<float4>();
    }

    void Reload()
    {
        std::ifstream file(ENGINE_SOURCE_PATH "/Resources/EditorConfig.json");
        j = nlohmann::json::parse(file);
    }

private:
    nlohmann::json j;
};
