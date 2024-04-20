#pragma once
#include "Core/Object.hpp"
class Object;
class Scene;
class GameLoop;
namespace Editor
{
class EditorState
{
public:
    static SRef<Object> selectedObject;
    static Scene* activeScene;
    static GameLoop* gameLoop;
};
} // namespace Editor
