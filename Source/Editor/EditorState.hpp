#pragma once
#include "Engine/Core/Object.hpp"
#include "Engine/Core/Ptr.hpp"
#include <span>
class Object;
class Scene;
class GameLoop;
class GameObject;
namespace Editor
{
class EditorState
{
public:
    static void SelectObject(ObjPtr<Object> obj, bool multiSelect = false);
    static Object* GetMainSelectedObject();
    static void DeselectObject(Object* obj);
    static std::vector<ObjPtr<Object>> GetSelectedObjects();

    static GameLoop*& GetGameLoop();

    static void Clear()
    {
        GetGameLoop() = nullptr;
        StaticGetSelectedObjects() = {};
    }

private:
    static std::vector<ObjPtr<Object>>& StaticGetSelectedObjects();
};
} // namespace Editor
