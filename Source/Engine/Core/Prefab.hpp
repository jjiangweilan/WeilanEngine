#pragma once
#include "Core/Asset.hpp"
#include "Core/GameObject.hpp"

class Prefab : public Asset
{
public:
    void SetGameObject();

private:
    int rootGameObjectIndex = -1;
    std::vector<GameObject> gameObjects;
};
