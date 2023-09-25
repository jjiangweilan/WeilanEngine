#pragma once
#include "Core/Object.hpp"
#include "InspectorRegistry.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{

// all inspector should be stateless, same inspector instance is reused for different object in GameEditor
class InspectorBase
{
public:
    virtual ~InspectorBase(){};
    virtual void DrawInspector() = 0;
    virtual void SetTarget(Object& obj) = 0;
};

template <class T>
class Inspector : public InspectorBase
{
public:
    void SetTarget(Object& obj) override
    {
        target = static_cast<T*>(&obj);
    }
    void DrawInspector() override {
        // default inspector
        ImGui::Text("%s", ((Object*)target)->GetUUID().ToString().c_str());
    };

protected:
    T* target;
};
} // namespace Engine::Editor
