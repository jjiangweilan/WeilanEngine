#pragma once
#include "Engine/Core/Asset.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include "Engine/Library/DynamicArray.hpp"

class GameObject;

class Animation : public Asset
{
    DECLARE_ASSET();

public:
    Animation() {};

    struct PositionKeyFrame
    {
        double time;
        glm::vec3 val;
    };
    struct RotationKeyFrame
    {
        double time;
        glm::quat val;
    };
    struct ScalingKeyFrame
    {
        double time;
        glm::vec3 val;
    };

    struct Channel
    {
        std::string nodeName;                    // name of the GameObject or Bone in a Mesh in the engine sense
        std::vector<PositionKeyFrame> positions; // the frame needs to be per unit time (1) right now
        std::vector<RotationKeyFrame> rotations;
        std::vector<ScalingKeyFrame> scalings;
    };

    struct AnimationClip
    {
        std::string name;
        float tickPerSecond;
        float duration;
        std::vector<Channel> channels;
    };

    using AnimationClips = std::unordered_map<std::string, std::unique_ptr<const AnimationClip>>;

    const AnimationClips& GetAnimationClips() { return clips; }

private:
    AnimationClips clips;
    friend struct ModelImporterImple;
};
