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
        Animation* anim = target->GetAnimation();

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

    void DrawAnimationSection(Animation*& anim)
    {
        EditorGUI::SeparatorTextLabeled("Animation");
        if (EditorGUI::ObjectField("Animation", anim))
        {
            target->Stop();
            target->SetAnimation(anim);
            SyncRootNameBuffer();
        }

        if (anim == nullptr)
        {
            ImGui::TextDisabled("Drop an Animation asset here to configure clips.");
            return;
        }

        const auto& clips = anim->GetAnimationClips();
        EditorGUI::TextFormatted("Clips", "%zu", clips.size());
    }

    void DrawPlaybackSection(Animation* anim)
    {
        EditorGUI::SeparatorTextLabeled("Playback");

        float speed = target->GetSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.0f, 4.0f))
        {
            target->SetSpeed(speed);
        }

        const Animation::AnimationClip* activeClip = target->GetActiveClip();
        ImGui::Text("Status: %s", target->IsPlaying() ? "Playing" : "Stopped");
        ImGui::Text("Active Clip: %s", activeClip != nullptr ? activeClip->name.c_str() : "None");

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

        ImGui::Text("Status: %s", target->IsRootMotionEnabled() ? "Enabled" : "Disabled");
        if (rootNameBuffer[0] == '\0')
        {
            ImGui::TextDisabled("Set a root GameObject name to enable root motion.");
        }
    }

    void DrawClipSection(Animation* anim)
    {
        EditorGUI::SeparatorTextLabeled("Clips");

        if (anim == nullptr)
        {
            ImGui::TextDisabled("No animation assigned.");
            return;
        }

        const auto& clips = anim->GetAnimationClips();
        if (clips.empty())
        {
            ImGui::TextDisabled("Animation has no clips.");
            return;
        }

        float blendFactor = target->GetBlendClipFactor();
        if (ImGui::DragFloat("Blend Factor", &blendFactor, 0.03f, 0.0f, 1.0f))
        {
            target->SetBlendClipFactor(blendFactor);
        }

        if (!ImGui::BeginTable("AnimationClips", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            return;

        ImGui::TableSetupColumn("Clip");
        ImGui::TableSetupColumn("Duration");
        ImGui::TableSetupColumn("TPS");
        ImGui::TableSetupColumn("Channels");
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        int id = 0;
        for (const auto& clipPair : clips)
        {
            const Animation::AnimationClip* clip = clipPair.second.get();
            bool active = target->GetActiveClip() == clip;
            bool blend = target->GetBlendClip() == clip;

            ImGui::PushID(id++);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (active || blend)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(active ? ImGuiCol_Header : ImGuiCol_HeaderHovered));
            ImGui::TextUnformatted(clip->name.c_str());

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
            if (ImGui::SmallButton("Set Active"))
                target->SetClip(clip->name);
            if (active)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (blend)
                ImGui::BeginDisabled();
            if (ImGui::SmallButton("Set Blend"))
                target->SetBlendClip(clip->name);
            if (blend)
                ImGui::EndDisabled();

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    float GetClipDurationSeconds(const Animation::AnimationClip& clip) const
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
