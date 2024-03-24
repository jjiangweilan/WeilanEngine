#pragma once
class Object;
class Scene;
class GameLoop;
namespace Editor
{
class EditorState
{
public:
    static Object* selectedObject;
    static Scene* activeScene;
    static GameLoop* gameLoop;
};
} // namespace Editor
