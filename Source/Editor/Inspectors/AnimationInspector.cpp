#include "Editor/EditorGUI.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/System/Rendering/AnimationClip.hpp"
#include "Engine/Runtime/System/Rendering/AnimationSet.hpp"

namespace Editor
{
class AnimationClipInspector : public Inspector<AnimationClip>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<AnimationClip>::DrawInspector(editor);

        ImGui::Text("Duration: %.2fs", target->GetDurationSeconds());
        ImGui::Text("Ticks Per Second: %.2f", target->tickPerSecond);
        ImGui::Text("Channels: %zu", target->channels.size());
    }

private:
    static const char _register;
};

const char AnimationClipInspector::_register = InspectorRegistry::Register<AnimationClipInspector, AnimationClip>();

class AnimationSetInspector : public Inspector<AnimationSet>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<AnimationSet>::DrawInspector(editor);

        ImGui::Text("Clips");
        ImGui::Indent();
        for (const ObjPtr<AnimationClip>& clip : target->GetClips())
        {
            ImGui::Text("%s", clip ? clip->GetName().c_str() : "Missing Clip");
        }
        ImGui::Unindent();
    }

private:
    static const char _register;
};

const char AnimationSetInspector::_register = InspectorRegistry::Register<AnimationSetInspector, AnimationSet>();

} // namespace Editor
