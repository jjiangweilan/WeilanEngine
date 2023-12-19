#pragma once
class Object;
class Scene;
namespace Editor
{
class EditorState
{
public:
    static Object* selectedObject;
    static Scene* activeScene;
};
} // namespace Editor
