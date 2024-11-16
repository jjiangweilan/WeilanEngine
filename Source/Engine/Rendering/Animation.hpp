#pragma once
#include "Core/Asset.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;

class Animation : public Asset
{
    DECLARE_ASSET();

public:
    Animation();

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
        std::string nodeName;                    // name of the GameObject or Bone in a Mesh in the engine sense
        std::vector<PositionKeyFrame> positions; // the frame needs to be per unit time (1) right now
        std::vector<RotationKeyFrame> rotations;
        std::vector<ScalingKeyFrame> scalings;
        Channel();
        Channel(Channel&& other);
        Channel(const Channel& other);
    };

    struct AnimationClip
    {
        std::string name;
        float tickPerSecond;
        float duration;
        std::vector<Channel> channels;
    };

    using AnimationClips = std::unordered_map<std::string, std::shared_ptr<const AnimationClip>>;

    const AnimationClips& GetAnimationClips() { return clips; }

private:
    AnimationClips clips;
    friend struct ModelImporterImple;
};
