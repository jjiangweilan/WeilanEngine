#pragma once
#include "Core/Ptr.hpp"
#include "Libs/DynamicArray.hpp"

class GameObject;
class SceneNode
{
public:

private:
    ObjPtr<GameObject> parent;
    DynamicArray<ObjPtr<GameObject>> children;
};
