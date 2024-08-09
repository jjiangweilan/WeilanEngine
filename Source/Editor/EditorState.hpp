#pragma once
#include "Core/Object.hpp"
#include <span>
class Object;
class Scene;
class GameLoop;
namespace Editor
{
class EditorState
{
public:
    static void SelectObject(SRef<Object> obj, bool multiSelect = false);
    static Object* GetMainSelectedObject();
    static void DeselectObject(Object* obj);
    static std::vector<SRef<Object>> GetSelectedObjects();

    static Scene* activeScene;
    static GameLoop* gameLoop;

private:
    static std::vector<SRef<Object>> selectedObjects;
};
} // namespace Editor
