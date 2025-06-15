#pragma once
#include "Core/Object.hpp"
#include "Core/Ptr.hpp"
#include <span>
class Object;
class Scene;
class GameLoop;
namespace Editor
{
class EditorState
{
public:
    static void SelectObject(ObjPtr<Object> obj, bool multiSelect = false);
    static Object* GetMainSelectedObject();
    static void DeselectObject(Object* obj);
    static DynamicArray<ObjPtr<Object>> GetSelectedObjects();

    static GameLoop* gameLoop;

    static void Clear()
    {
        gameLoop = nullptr;
        selectedObjects = {};
    }

private:
    static DynamicArray<ObjPtr<Object>> selectedObjects;
};
} // namespace Editor
