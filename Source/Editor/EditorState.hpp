#pragma once
#include "Engine/Core/Object.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Editor/UndoManager.hpp"
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
    static bool CanSelectPreviousObject();
    static void SelectPreviousObject();
    static void DeselectObject(Object* obj);
    static std::vector<ObjPtr<Object>> GetSelectedObjects();

    static GameLoop*& GetGameLoop();
    static UndoManager& GetUndoManager();
    static bool& GetScaleLock();
    static bool& GetPositionChildrenWorldLock();

    static void Clear()
    {
        GetGameLoop() = nullptr;
        StaticGetSelectedObjects() = {};
        StaticGetSelectionHistory() = {};
        GetUndoManager().Clear();
    }

private:
    static std::vector<ObjPtr<Object>>& StaticGetSelectedObjects();
    static std::vector<ObjPtr<Object>>& StaticGetSelectionHistory();
    static void PushSelectionHistory(ObjPtr<Object> obj);
};
} // namespace Editor
