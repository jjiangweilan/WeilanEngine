#pragma once
#include "Core/Ptr.hpp"
#include <vector>

class GameObject;
class SceneNode
{
public:

private:
    ObjPtr<GameObject> parent;
    std::vector<ObjPtr<GameObject>> children;
};
