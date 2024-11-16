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
        auto clip = target->GetActiveClip();

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
            for (auto clip : clips)
            {
                if(ImGui::Button("Activate"))
                {
                    target->PlayAnimation(clip.second->name, 0, -1);
                }
                ImGui::SameLine();
                ImGui::Text("%s", clip.second->name.c_str());
            }
            ImGui::Unindent();
        }
    }

private:
    static const char _register;
};

const char AnimationPlayerInspector::_register =
    InspectorRegistry::Register<AnimationPlayerInspector, AnimationPlayer>();

} // namespace Editor
