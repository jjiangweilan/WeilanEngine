#pragma once
namespace Engine
{
class Object;
class Scene;
} // namespace Engine
namespace Engine::Editor
{
class EditorState
{
public:
    static Object* selectedObject;
    static Scene* activeScene;
};
} // namespace Engine::Editor
