#pragma once

#include "Engine/Core/Asset.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

class AnimationClip : public Asset
{
    DECLARE_ASSET();

public:
    struct PositionKeyFrame
    {
        double time = 0.0;
        glm::vec3 val = glm::vec3(0.0f);
    };

    struct RotationKeyFrame
    {
        double time = 0.0;
        glm::quat val = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

    struct ScalingKeyFrame
    {
        double time = 0.0;
        glm::vec3 val = glm::vec3(1.0f);
    };

    struct Channel
    {
        std::string nodeName;
        std::vector<PositionKeyFrame> positions;
        std::vector<RotationKeyFrame> rotations;
        std::vector<ScalingKeyFrame> scalings;
    };

    float tickPerSecond = 0.0f;
    float duration = 0.0f;
    std::vector<Channel> channels;

    float GetDurationSeconds() const
    {
        if (tickPerSecond <= 0.0f)
            return 0.0f;
        return duration / tickPerSecond;
    }
};
