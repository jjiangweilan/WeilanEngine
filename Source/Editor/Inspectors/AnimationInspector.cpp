#include "Editor/EditorGUI.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/System/Rendering/Animation.hpp"

namespace Editor
{
class AnimationInspector : public Inspector<Animation>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Animation>::DrawInspector(editor);

        ImGui::Text("Clips");
        ImGui::Indent();
        auto& clips = target->GetAnimationClips();
        for (auto& clip : clips)
        {
            ImGui::Text("%s", clip.second->name.c_str());
        }
        ImGui::Unindent();
    }

private:
    static const char _register;
};

const char AnimationInspector::_register = InspectorRegistry::Register<AnimationInspector, Animation>();

} // namespace Editor
