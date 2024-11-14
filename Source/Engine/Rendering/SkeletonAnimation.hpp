#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SkeletonBone
{
    std::vector<std::unique_ptr<SkeletonBone>> children;
    SkeletonBone* parent;
    std::string name;
    glm::mat4 offsetMatrix;

    glm::vec3 translation = glm::vec3(0.0);
    glm::vec3 scaling = glm::vec3(1.0, 1.0, 1.0);
    glm::quat rotation = glm::quat(1, 0, 0, 0);
    glm::mat4 finalTransformMatrix;
    glm::mat4 transformMatrix;
    size_t boneId;

public:
    const std::vector<std::shared_ptr<SkeletonBone>> GetChildren() const;
    SkeletonBone* GetParent() const;
    const std::string& GetName() const;
    const glm::mat4& GetOffsetMatrix() const;
    const size_t& GetBoneId() const;
    const glm::mat4 GetTransformMatrix() const;

    SkeletonBone();
    SkeletonBone(SkeletonBone&& other);
    SkeletonBone(const SkeletonBone& other);
    SkeletonBone& operator=(const SkeletonBone& bone);

    friend class ModelImporter;
    friend class SkeletonAnimation;
};

class SkeletonAnimation
{
public:
    struct PositionKeyFrame
    {
        double time;
        glm::vec3 val;
        PositionKeyFrame(const double& time, const glm::vec3& val) : time(time), val(val) {}
    };
    struct RotationKeyFrame
    {
        double time;
        glm::quat val;
        RotationKeyFrame(const double& time, const glm::quat& val) : time(time), val(val) {}
    };
    struct ScalingKeyFrame
    {
        double time;
        glm::vec3 val;
        ScalingKeyFrame(const double& time, const glm::vec3& val) : time(time), val(val) {}
    };
    struct Channel
    {
        size_t boneId;
        std::vector<PositionKeyFrame> positions; // the frame needs to be per unit time (1) right now
        std::vector<RotationKeyFrame> rotations;
        std::vector<ScalingKeyFrame> scalings;
        Channel();
        Channel(Channel&& other);
        Channel(const Channel& other);
    };
    struct Animation
    {
        float tickPerSecond;
        float duration;
        std::vector<Channel> channels;
        Animation(const float& tickPerSecond, std::vector<Channel>&& channels, const float& duration)
            : tickPerSecond(tickPerSecond), duration(duration), channels(std::move(channels)) {};
    };

    SkeletonAnimation();
    bool PlayAnimation(const std::string& animationName, const int& startFrame, const int& endFrame);
    void UpdateBonesTransform(std::vector<std::unique_ptr<SkeletonBone>>& bones);
    void UpdateBonesRoot(std::vector<SkeletonBone*>& bones);

private:
    void UpdateBonesRootHelper(SkeletonBone* bones, glm::mat4 parentMatrix);
    std::unordered_map<std::string, Animation> animations;
    Animation* currentAnimation;
    double timePassed = 0;
    int currentStartFrame;
    int currentEndFrame;
    friend class ModelImporter;
};

class Skeleton
{
    std::unique_ptr<SkeletonAnimation> animation;
    mutable std::vector<std::unique_ptr<SkeletonBone>> bones;
    mutable std::vector<SkeletonBone*> rootBones;

public:
    Skeleton();
    Skeleton(Skeleton&& other);
    Skeleton(const Skeleton& other);
    Skeleton& operator=(const Skeleton& other);

    std::vector<SkeletonBone*>& GetRootBones() const;
    SkeletonAnimation* GetAnimation() const;
    std::vector<std::unique_ptr<SkeletonBone>>& GetBones() const;
    void UpdateBoneTransformMatrix();

    friend class ModelImporter;
};
