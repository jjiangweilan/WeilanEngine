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
    static std::vector<ObjPtr<Object>> GetSelectedObjects();

    static ObjPtr<Scene> activeScene;
    static GameLoop* gameLoop;

    static void Clear()
    {
        activeScene = nullptr;
        gameLoop = nullptr;
        selectedObjects = {};
    }

private:
    static std::vector<ObjPtr<Object>> selectedObjects;
};
} // namespace Editor
