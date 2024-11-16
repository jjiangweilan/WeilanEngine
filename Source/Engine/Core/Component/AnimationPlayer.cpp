#include "AnimationPlayer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Time.hpp"

DEFINE_OBJECT(AnimationPlayer, "F1093426-DC3A-45F6-9C3B-B7CFA098285A");

AnimationPlayer::AnimationPlayer() : Component(nullptr) {};
AnimationPlayer::AnimationPlayer(GameObject* gameObject) : Component(gameObject) {};

const std::string& AnimationPlayer::GetName()
{
    static std::string name = "AnimationPlayer";
    return name;
}

void AnimationPlayer::TickAnimation()
{
    if (animatedObjects.empty() || currentClip == nullptr || !isPlaying)
        return;

    auto& animation = *currentClip;
    size_t index;
    float a;
    float frame =
        std::fmod(timePassed * animation.tickPerSecond, currentEndFrame - currentStartFrame) + currentStartFrame;

    for (auto& channel : animation.channels)
    {
        auto& bone = animatedObjects.at(channel.runtimeNodeId);
        // position
        index = 0;
        if (frame > channel.positions.front().time)
        {
            if (frame <= channel.positions.back().time)
            {
                while (frame > channel.positions[index].time)
                    index++;
                auto p1 = channel.positions[index - 1];
                auto p2 = channel.positions[index];
                float a = (frame - p1.time) / (p2.time - p1.time);
                bone->SetLocalPosition((1 - a) * p1.val + a * p2.val);
            }
            else
                bone->SetLocalPosition(channel.positions.back().val);
        }
        else
            bone->SetLocalPosition(channel.positions.front().val);

        index = 0;
        if (frame > channel.rotations.front().time)
        {
            if (frame <= channel.rotations.back().time)
            {
                while (frame > channel.rotations[index].time)
                    index++;
                auto r1 = channel.rotations[index - 1];
                auto r2 = channel.rotations[index];
                a = (frame - r1.time) / (r2.time - r1.time);
                bone->SetLocalRotation(glm::slerp(r1.val, r2.val, a)); // slerp to ensure shortest path is taken
            }
            else
                bone->SetLocalRotation(channel.rotations.back().val);
        }
        else
            bone->SetLocalRotation(channel.rotations.front().val);

        index = 0;
        if (frame > channel.scalings.front().time)
        {
            if (frame <= channel.scalings.back().time)
            {
                while (frame > channel.scalings[index].time)
                    index++;
                auto s1 = channel.scalings[index - 1];
                auto s2 = channel.scalings[index];
                a = (frame - s1.time) / (s2.time - s1.time);
                bone->SetLocalScale((1 - a) * s1.val + a * s2.val);
            }
            else
                bone->SetLocalScale(channel.scalings.back().val);
        }
        else
            bone->SetLocalScale(channel.scalings.front().val);
    }
    timePassed += Time::DeltaTime();
}

bool AnimationPlayer::SetClip(const std::string& animationName, GameObject* root, int startFrame, int endFrame)
{
    isPlaying = false;
    auto iter = animation->GetAnimationClips().find(animationName);
    if (iter != animation->GetAnimationClips().end())
    {
        currentClip = iter->second.get();
        timePassed = 0;
        currentStartFrame = startFrame;
        if (endFrame <= 0)
        {
            currentEndFrame = currentClip->duration - endFrame;
        }
        else
        {
            currentEndFrame = endFrame;
        }
    }
    else
    {
        currentClip = nullptr;
        return false;
    }

    currentClip = nullptr;
    timePassed = 0;
    currentStartFrame = 0;
    currentEndFrame = -1;

    return true;
}

void AnimationPlayer::Stop()
{
    isPlaying = true;
}

void AnimationPlayer::Play()
{
    isPlaying = false;
}
