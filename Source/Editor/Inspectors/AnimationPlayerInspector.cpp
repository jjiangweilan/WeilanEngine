#include "Inspector.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/EditorGUI.hpp"
#include <algorithm>
#include <cstdio>
#include <string.h>

namespace Editor
{
class AnimationPlayerInspector : public Inspector<AnimationPlayer>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<AnimationPlayer>::OnEnable(obj);

        SyncRootNameBuffer();
    }

    void DrawInspector(GameEditor& editor) override
    {
        AnimationSet* anim = target->GetAnimationSet();

        DrawAnimationSection(anim);
        DrawPlaybackSection(anim);
        DrawRootMotionSection();
        DrawClipSection(anim);

        if (ImGui::TreeNode("Auto Inspector"))
        {
            EditorGUI::AutoObjectInspector(target, true);
            ImGui::TreePop();
        }
    }

private:
    char rootNameBuffer[256];
    static const char _register;

    void SyncRootNameBuffer()
    {
        const std::string& rootName = target->GetRootName();
        size_t copySize = std::min(rootName.size(), sizeof(rootNameBuffer) - 1);
        memcpy(rootNameBuffer, rootName.data(), copySize);
        rootNameBuffer[copySize] = '\0';
    }

    void DrawAnimationSection(AnimationSet*& anim)
    {
        EditorGUI::SeparatorTextLabeled("Animation Set");
        if (EditorGUI::ObjectField("Animation Set", anim))
        {
            target->Stop();
            target->SetAnimationSet(anim);
            SyncRootNameBuffer();
        }

        if (anim == nullptr)
        {
            ImGui::TextDisabled("Drop an AnimationSet asset here to configure clips.");
            return;
        }

        const auto& clips = anim->GetClips();
        EditorGUI::TextFormatted("Clips", "%zu", clips.size());
    }

    void DrawPlaybackSection(AnimationSet* anim)
    {
        EditorGUI::SeparatorTextLabeled("Playback");

        float speed = target->GetSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.0f, 4.0f))
        {
            target->SetSpeed(speed);
        }

        bool autoPlay = target->IsAutoPlay();
        if (ImGui::Checkbox("Auto Play", &autoPlay))
        {
            target->SetAutoPlay(autoPlay);
        }

        const AnimationClip* activeClip = target->GetActiveClip();
        ImGui::Text("Status: %s", target->IsPlaying() ? "Playing" : "Stopped");
        ImGui::Text("Active Clip: %s", activeClip != nullptr ? activeClip->GetName().c_str() : "None");

        float duration = target->GetMainClipDurationInSeconds();
        if (activeClip != nullptr && duration > 0.0f)
        {
            float time = target->GetMainClipTimePassed();
            float progress = std::clamp(time / duration, 0.0f, 1.0f);
            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "%.2fs / %.2fs", time, duration);
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), overlay);
        }

        bool canPlay = anim != nullptr && activeClip != nullptr;
        if (!canPlay)
            ImGui::BeginDisabled();
        if (ImGui::Button("Play"))
            target->Play();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            target->Stop();
        if (!canPlay)
            ImGui::EndDisabled();
    }

    void DrawRootMotionSection()
    {
        EditorGUI::SeparatorTextLabeled("Root Motion");

        if (ImGui::InputText("Root", rootNameBuffer, sizeof(rootNameBuffer)))
        {
            target->SetRootMotionRoot(rootNameBuffer);
        }

        const char* translationModeLabels[] = {"None", "Horizontal", "Vertical", "Full"};
        int translationMode = static_cast<int>(target->GetRootMotionTranslationMode());
        if (ImGui::Combo("Translation", &translationMode, translationModeLabels, IM_ARRAYSIZE(translationModeLabels)))
            target->SetRootMotionTranslationMode(static_cast<RootMotionTranslationMode>(translationMode));

        const char* rotationModeLabels[] = {"None", "Yaw", "Full"};
        int rotationMode = static_cast<int>(target->GetRootMotionRotationMode());
        if (ImGui::Combo("Rotation", &rotationMode, rotationModeLabels, IM_ARRAYSIZE(rotationModeLabels)))
            target->SetRootMotionRotationMode(static_cast<RootMotionRotationMode>(rotationMode));

        ImGui::Text("Status: %s", target->IsRootMotionEnabled() ? "Enabled" : "Disabled");
        if (rootNameBuffer[0] == '\0')
        {
            ImGui::TextDisabled("Set a root GameObject name to enable root motion.");
        }

        const RootMotionDelta& delta = target->GetRootMotionDelta();
        ImGui::Text(
            "Unconsumed Translation: %.3f, %.3f, %.3f",
            delta.translation.x,
            delta.translation.y,
            delta.translation.z
        );
        ImGui::Text(
            "Unconsumed Local Translation: %.3f, %.3f, %.3f",
            delta.localTranslation.x,
            delta.localTranslation.y,
            delta.localTranslation.z
        );
    }

    void DrawClipSection(AnimationSet* anim)
    {
        EditorGUI::SeparatorTextLabeled("Clips");

        if (anim == nullptr)
        {
            ImGui::TextDisabled("No animation set assigned.");
            return;
        }

        const auto& clips = anim->GetClips();
        if (clips.empty())
        {
            ImGui::TextDisabled("AnimationSet has no clips.");
            return;
        }

        float blendFactor = target->GetBlendClipFactor();
        if (ImGui::DragFloat("Blend Factor", &blendFactor, 0.03f, 0.0f, 1.0f))
        {
            target->SetBlendClipFactor(blendFactor);
        }

        if (!ImGui::BeginTable(
                "AnimationClips",
                6,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
            ))
            return;

        ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("TPS", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 2.f);
        ImGui::TableHeadersRow();

        int id = 0;
        for (const auto& clipPair : clips)
        {
            const AnimationClip* clip = clipPair;
            if (clip == nullptr)
                continue;
            bool active = target->GetActiveClip() == clip;
            bool blend = target->GetBlendClip() == clip;

            ImGui::PushID(id++);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (active || blend)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(active ? ImGuiCol_Header : ImGuiCol_HeaderHovered));
            ImGui::TextUnformatted(clip->GetName().c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2fs", GetClipDurationSeconds(*clip));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", clip->tickPerSecond);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", clip->channels.size());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(GetClipRole(active, blend));

            ImGui::TableSetColumnIndex(5);
            if (active)
                ImGui::BeginDisabled();
            if (ImGui::SmallButton("Activate"))
                target->SetClip(clip->GetName());
            if (active)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::SmallButton("Blend"))
            {
                if (blend)
                    target->ClearBlendClip();
                else
                    target->SetBlendClip(clip->GetName());
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    float GetClipDurationSeconds(const AnimationClip& clip) const
    {
        if (clip.tickPerSecond <= 0.0f)
            return 0.0f;
        return clip.duration / clip.tickPerSecond;
    }

    const char* GetClipRole(bool active, bool blend) const
    {
        if (active && blend)
            return "Active + Blend";
        if (active)
            return "Active";
        if (blend)
            return "Blend";
        return "Unused";
    }
};

const char AnimationPlayerInspector::_register =
    InspectorRegistry::Register<AnimationPlayerInspector, AnimationPlayer>();

} // namespace Editor
