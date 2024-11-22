#include "../EditorState.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "EditorGUI.hpp"
#include "Inspector.hpp"
#include <string.h>

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

        float speed = target->GetSpeed();
        if (ImGui::SliderFloat("speed", &speed, 0, 4))
        {
            target->SetSpeed(speed);
        }

        const std::string& rootMotion = target->GetRootName();
        memcpy(rootNameBuffer, rootMotion.data(), rootMotion.size() < 256 ? rootMotion.size() : 256);
        if (ImGui::InputText("Root Motion", rootNameBuffer, 256))
        {
            target->SetRoot(rootNameBuffer);
            if (rootNameBuffer[0] != '\0')
            {
                target->SetRootMotionEnabled(true);
            }
            else
            {
                target->SetRootMotionEnabled(false);
            }
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
                if (ImGui::Button("Set as BlendClip"))
                {
                    target->SetBlendClip(clip.second->name);
                }

                bool active = target->GetActiveClip() == clip.second.get();
                bool blend = target->GetBlendClip() == clip.second.get();

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    active ? ImVec4{0, 1, 0, 1} : (blend ? ImVec4{1, 1, 0, 1} : ImVec4{1, 0, 0, 1})
                );
                ImGui::Text("%s", clip.second->name.c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }

            float blendFactor = target->GetBlendClipFactor();
            if (ImGui::DragFloat("Blend Factor", &blendFactor, 0.03, 0.f, 1.0f))
            {
                target->SetBlendClipFactor(blendFactor);
            }

            ImGui::Unindent();

            if (ImGui::Button("Play"))
                target->Play();

            if (ImGui::Button("Stop"))
                target->Stop();
        }
    }

private:
    char rootNameBuffer[256];
    static const char _register;
};

const char AnimationPlayerInspector::_register =
    InspectorRegistry::Register<AnimationPlayerInspector, AnimationPlayer>();

} // namespace Editor
