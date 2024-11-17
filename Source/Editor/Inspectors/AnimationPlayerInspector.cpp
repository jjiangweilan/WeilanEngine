#include "../EditorState.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "EditorGUI.hpp"
#include "Inspector.hpp"

namespace Editor
{
class AnimationPlayerInspector : public Inspector<AnimationPlayer>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Animation* anim = target->GetAnimation();

        std::string animationFieldText = anim ? anim->GetName() : "Animation(none)";
        if (GUI::ObjectField(animationFieldText, anim))
        {
            target->Stop();
            target->SetAnimation(anim);
        }

        if (anim)
        {
            ImGui::Indent();
            auto& clips = anim->GetAnimationClips();
            int id = 0;
            for (auto& clip : clips)
            {
                ImGui::PushID(id++);
                if (ImGui::Button("Activate"))
                {
                    target->SetClip(clip.second->name);
                }
                ImGui::SameLine();
                bool active = target->GetActiveClip() == clip.second.get();

                ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4{0, 1, 0, 1} : ImVec4{1, 0, 0, 1});
                ImGui::Text("%s", clip.second->name.c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Unindent();

            if (ImGui::Button("Play"))
                target->Play();

            if (ImGui::Button("Stop"))
                target->Stop();
        }
    }

private:
    static const char _register;
};

const char AnimationPlayerInspector::_register =
    InspectorRegistry::Register<AnimationPlayerInspector, AnimationPlayer>();

} // namespace Editor
