#include "SkeletonAnimation.hpp"
#include "Core/Time.hpp"
#include "glm/gtx/quaternion.hpp"

SkeletonAnimation::Channel::Channel() : boneId(0), positions(), rotations(), scalings() {}
SkeletonAnimation::Channel::Channel(Channel&& other)
{
    boneId = other.boneId;
    positions = std::move(other.positions);
    rotations = std::move(other.rotations);
    scalings = std::move(other.scalings);
}
SkeletonAnimation::Channel::Channel(const Channel& other)
{
    boneId = other.boneId;
    positions = other.positions;
    rotations = other.rotations;
    scalings = other.scalings;
}

SkeletonAnimation::SkeletonAnimation() : currentAnimation(nullptr), currentStartFrame(0), currentEndFrame(-1) {}

void SkeletonAnimation::UpdateBonesTransform(std::vector<std::unique_ptr<SkeletonBone>>& bones)
{
    if (currentAnimation == nullptr)
        return;
    auto& animation = *currentAnimation;

    size_t index;
    float a;
    float frame =
        std::fmod(timePassed * animation.tickPerSecond, currentEndFrame - currentStartFrame) + currentStartFrame;

    for (auto& channel : animation.channels)
    {
        auto& bone = bones[channel.boneId];
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
                bone->translation = (1 - a) * p1.val + a * p2.val;
            }
            else
                bone->translation = channel.positions.back().val;
        }
        else
            bone->translation = channel.positions.front().val;

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
                bone->rotation = glm::slerp(r1.val, r2.val, a); // slerp to ensure shortest path is taken
            }
            else
                bone->rotation = channel.rotations.back().val;
        }
        else
            bone->rotation = channel.rotations.front().val;

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
                bone->scaling = (1 - a) * s1.val + a * s2.val;
            }
            else
                bone->scaling = channel.scalings.back().val;
        }
        else
            bone->scaling = channel.scalings.front().val;
    }

    timePassed += Time::DeltaTime();
}

bool SkeletonAnimation::PlayAnimation(const std::string& animationName, const int& startFrame, const int& endFrame)
{
    auto iter = animations.find(animationName);
    if (iter != animations.end())
    {
        currentAnimation = &iter->second;
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

void SkeletonAnimation::UpdateBonesRoot(std::vector<SkeletonBone*>& roots)
{
    for (auto root : roots)
    {
        UpdateBonesRootHelper(root, glm::mat4(1.0));
    }
}

void SkeletonAnimation::UpdateBonesRootHelper(SkeletonBone* bone, glm::mat4 parentMatrix)
{
    auto transform = glm::translate(glm::mat4(1.0), bone->translation) * glm::toMat4(bone->rotation) *
                     glm::scale(glm::mat4(1.0), bone->scaling);

    bone->finalTransformMatrix =
        glm::inverse(bone->offsetMatrix) * glm::inverse(bone->transformMatrix) * transform * bone->offsetMatrix;
    if (bone->parent != nullptr)
    {
        bone->finalTransformMatrix = parentMatrix * bone->finalTransformMatrix;
    }

    for (auto& child : bone->children)
    {
        UpdateBonesRootHelper(child.get(), bone->finalTransformMatrix);
    }
}
Skeleton::Skeleton() : animation(nullptr), rootBones() {}
Skeleton::Skeleton(Skeleton&& other)
{
    animation = std::move(other.animation);
    rootBones = std::move(other.rootBones);
    bones = std::move(other.bones);
}

Skeleton::Skeleton(const Skeleton& other)
{
    animation = std::make_unique<SkeletonAnimation>(*other.animation);
    rootBones = other.rootBones;
    bones = other.bones;
}
Skeleton& Skeleton::operator=(const Skeleton& other)
{
    animation = other.animation;
    rootBones = other.rootBones;
    bones = other.bones;
    return *this;
}

std::vector<Bone*>& Skeleton::GetRootBones() const
{
    return rootBones;
}
SkeletonAnimation* Skeleton::GetAnimation() const
{
    return animation.get();
}

void Skeleton::UpdateBoneTransformMatrix() {}

std::vector<std::shared_ptr<Bone>>& Skeleton::GetBones() const
{
    return bones;
}
