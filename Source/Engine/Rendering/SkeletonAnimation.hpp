#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;
struct SkeletonBone
{
    std::string name;
    glm::mat4 offsetMatrix;
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
        int runtimeBoneId = 0;
        std::string boneName;
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

    // returns final transform matrices of each bone
    const std::vector<glm::mat4>& TickAnimation();

    // let boneObjects only lives in runtime
    bool Initialize(std::vector<std::pair<GameObject*, SkeletonBone*>> bones);
    void Deinit();

private:
    std::unordered_map<std::string, std::shared_ptr<const Animation>> animations;

    const Animation* currentAnimation;
    std::vector<std::pair<GameObject*, SkeletonBone*>> bones;
    std::vector<glm::mat4> finalTransformMatrices;
    double timePassed = 0;
    int currentStartFrame;
    int currentEndFrame;

    friend struct ModelImporterImple;
};
