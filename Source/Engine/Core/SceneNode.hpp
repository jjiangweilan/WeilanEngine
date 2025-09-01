#pragma once
#include "Core/Ptr.hpp"
#include "Libs/DynamicArray.hpp"

class GameObject;
class SceneNode
{
public:

private:
    ObjPtr<GameObject> parent;
    std::vector<ObjPtr<GameObject>> children;
};
