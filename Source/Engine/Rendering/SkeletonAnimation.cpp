#include "SkeletonAnimation.hpp"
#include "Core/GameObject.hpp"
#include "Core/Time.hpp"
#include "glm/gtx/quaternion.hpp"

SkeletonAnimation::Channel::Channel() : positions(), rotations(), scalings() {}
SkeletonAnimation::Channel::Channel(Channel&& other)
{
    positions = std::move(other.positions);
    rotations = std::move(other.rotations);
    scalings = std::move(other.scalings);
}
SkeletonAnimation::Channel::Channel(const Channel& other)
{
    positions = other.positions;
    rotations = other.rotations;
    scalings = other.scalings;
}

SkeletonAnimation::SkeletonAnimation() : currentAnimation(nullptr), currentStartFrame(0), currentEndFrame(-1) {}

const std::vector<glm::mat4>& SkeletonAnimation::TickAnimation()
{
    if (currentAnimation == nullptr)
        return finalTransformMatrices;

    auto& animation = *currentAnimation;
    size_t index;
    float a;
    float frame =
        std::fmod(timePassed * animation.tickPerSecond, currentEndFrame - currentStartFrame) + currentStartFrame;

    for (auto& channel : animation.channels)
    {
        auto& bone = bones.at(channel.runtimeBoneId).first;
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

    // update final transform matrices
    for (int boneIndex = 0; boneIndex < bones.size(); boneIndex++)
    {
        finalTransformMatrices[boneIndex] =
            bones[boneIndex].first->GetWorldMatrix() * this->bones[boneIndex].second->offsetMatrix;
    }

    return finalTransformMatrices;
}

bool SkeletonAnimation::PlayAnimation(const std::string& animationName, const int& startFrame, const int& endFrame)
{
    auto iter = animations.find(animationName);
    if (iter != animations.end())
    {
        currentAnimation = iter->second.get();
        timePassed = 0;
        currentStartFrame = startFrame;
        if (endFrame <= 0)
        {
            currentEndFrame = currentAnimation->duration - endFrame;
        }
        else
        {
            currentEndFrame = endFrame;
        }
    }
    else
    {
        currentAnimation = nullptr;
        return false;
    }
    return true;
}

bool SkeletonAnimation::Initialize(std::vector<std::pair<GameObject*, SkeletonBone*>> bones)
{
    this->bones = bones;
    currentAnimation = nullptr;
    timePassed = 0;
    currentStartFrame = 0;
    currentEndFrame = -1;

    return true;
}

void SkeletonAnimation::Deinit()
{
    bones.clear();
}
