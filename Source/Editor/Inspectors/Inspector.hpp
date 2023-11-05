#pragma once
#include "Core/Object.hpp"
#include "InspectorRegistry.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{

class GameEditor;
// all inspector should be stateless, same inspector instance is reused for different object in GameEditor
class InspectorBase
{
public:
    virtual ~InspectorBase(){};
    virtual void DrawInspector(GameEditor& editor) = 0;
    virtual void OnEnable(Object& obj) = 0;
};

template <class T>
class Inspector : public InspectorBase
{
public:
    void OnEnable(Object& obj) override
    {
        target = static_cast<T*>(&obj);
    }
    void DrawInspector(GameEditor& editor) override
    {
        // default inspector
        ImGui::Text("%s", ((Object*)target)->GetUUID().ToString().c_str());
    };

protected:
    T* target;
};
} // namespace Engine::Editor
