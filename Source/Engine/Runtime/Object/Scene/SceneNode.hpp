#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/DynamicArray.hpp"

class GameObject;
class SceneNode
{
public:

private:
    ObjPtr<GameObject> parent;
    std::vector<ObjPtr<GameObject>> children;
};
