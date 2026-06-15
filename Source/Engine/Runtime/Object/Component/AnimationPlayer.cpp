#include "AnimationPlayer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsScene.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtx/quaternion.hpp>

DEFINE_OBJECT(Component, AnimationPlayer, "F1093426-DC3A-45F6-9C3B-B7CFA098285A");

TYPE_REFLECTION_MEMBER_VARIABLES(
    AnimationPlayer,
    TYPE_REFLECTION_MEM(AnimationPlayer, animation),
    TYPE_REFLECTION_MEM(AnimationPlayer, speed),
    TYPE_REFLECTION_MEM(AnimationPlayer, rootName)
);

AnimationPlayer::AnimationPlayer() : Component(nullptr) {};
AnimationPlayer::AnimationPlayer(GameObject* gameObject) : Component(gameObject) {};

namespace
{
constexpr float RootMotionEpsilon = 0.00001f;

glm::quat NormalizeSafe(const glm::quat& q)
{
    if (glm::dot(q, q) <= RootMotionEpsilon)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return glm::normalize(q);
}

glm::vec3 SamplePosition(const Animation::Channel& channel, float currentTime)
{
    if (channel.positions.empty())
        return glm::vec3(0.0f);

    glm::vec3 position = channel.positions.front().val;
    if (currentTime > channel.positions.front().time)
    {
        if (currentTime <= channel.positions.back().time)
        {
            size_t index = 0;
            while (currentTime > channel.positions[index].time)
                index++;
            auto p1 = channel.positions[index - 1];
            auto p2 = channel.positions[index];
            float a = (currentTime - p1.time) / (p2.time - p1.time);
            position = (1 - a) * p1.val + a * p2.val;
        }
        else
        {
            position = channel.positions.back().val;
        }
    }

    return position;
}

glm::quat SampleRotation(const Animation::Channel& channel, float currentTime)
{
    if (channel.rotations.empty())
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    glm::quat rotation = channel.rotations.front().val;
    if (currentTime > channel.rotations.front().time)
    {
        if (currentTime <= channel.rotations.back().time)
        {
            size_t index = 0;
            while (currentTime > channel.rotations[index].time)
                index++;
            auto r1 = channel.rotations[index - 1];
            auto r2 = channel.rotations[index];
            float a = (currentTime - r1.time) / (r2.time - r1.time);
            rotation = glm::slerp(r1.val, r2.val, a);
        }
        else
        {
            rotation = channel.rotations.back().val;
        }
    }

    return NormalizeSafe(rotation);
}

glm::vec3 SampleScale(const Animation::Channel& channel, float currentTime)
{
    if (channel.scalings.empty())
        return glm::vec3(1.0f);

    glm::vec3 scale = channel.scalings.front().val;
    if (currentTime > channel.scalings.front().time)
    {
        if (currentTime <= channel.scalings.back().time)
        {
            size_t index = 0;
            while (currentTime > channel.scalings[index].time)
                index++;
            auto s1 = channel.scalings[index - 1];
            auto s2 = channel.scalings[index];
            float a = (currentTime - s1.time) / (s2.time - s1.time);
            scale = (1 - a) * s1.val + a * s2.val;
        }
        else
        {
            scale = channel.scalings.back().val;
        }
    }

    return scale;
}

glm::vec3 FilterTranslation(glm::vec3 translation, RootMotionTranslationMode mode)
{
    switch (mode)
    {
    case RootMotionTranslationMode::None:
        return glm::vec3(0.0f);
    case RootMotionTranslationMode::Horizontal:
        translation.y = 0.0f;
        return translation;
    case RootMotionTranslationMode::Vertical:
        return glm::vec3(0.0f, translation.y, 0.0f);
    case RootMotionTranslationMode::Full:
        return translation;
    }

    return glm::vec3(0.0f);
}

glm::quat FilterRotation(const glm::quat& rotation, RootMotionRotationMode mode)
{
    switch (mode)
    {
    case RootMotionRotationMode::None:
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    case RootMotionRotationMode::Yaw:
    {
        glm::vec3 euler = glm::eulerAngles(rotation);
        return NormalizeSafe(glm::angleAxis(euler.y, glm::vec3(0.0f, 1.0f, 0.0f)));
    }
    case RootMotionRotationMode::Full:
        return NormalizeSafe(rotation);
    }

    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}
} // namespace

const std::string& AnimationPlayer::GetName() const
{
    static std::string name = "AnimationPlayer";
    return name;
}

void AnimationPlayer::UpdateAnimatedGameObject(
    const Animation::AnimationClip& mainClip,
    float& timePassed,
    float& durationInSeconds,
    float tickPerSecond,
    float deltaTime,
    float blend,
    bool stripRootMotion
)
{
    float currentTime = timePassed * tickPerSecond;
    for (int channelIndex = 0; channelIndex < mainClip.channels.size(); ++channelIndex)
    {
        auto& channel = mainClip.channels[channelIndex];
        auto& bone = animatedObjects.at(channelIndex);

        glm::vec3 newPosition = SamplePosition(channel, currentTime);
        if (stripRootMotion && channelIndex == animatedRootGOIndex && !channel.positions.empty())
        {
            const glm::vec3 rootReferencePosition = channel.positions.front().val;
            switch (rootMotionTranslationMode)
            {
            case RootMotionTranslationMode::None:
                break;
            case RootMotionTranslationMode::Horizontal:
                newPosition.x = rootReferencePosition.x;
                newPosition.z = rootReferencePosition.z;
                break;
            case RootMotionTranslationMode::Vertical:
                newPosition.y = rootReferencePosition.y;
                break;
            case RootMotionTranslationMode::Full:
                newPosition = rootReferencePosition;
                break;
            }
        }
        bone.position = glm::mix(bone.position, newPosition, blend);

        glm::quat newRotation = SampleRotation(channel, currentTime);
        if (stripRootMotion && channelIndex == animatedRootGOIndex && rootMotionRotationMode != RootMotionRotationMode::None &&
            !channel.rotations.empty())
            newRotation = channel.rotations.front().val;
        bone.rotation = glm::slerp(bone.rotation, newRotation, blend);

        glm::vec3 newScale = SampleScale(channel, currentTime);
        bone.scale = glm::mix(bone.scale, newScale, blend);
    }

    AdvanceClipTime(timePassed, durationInSeconds, deltaTime);
}

void AnimationPlayer::TickAnimation()
{
    TickAnimation(Time::DeltaTime());
}

void AnimationPlayer::TickAnimation(float deltaTime)
{
    if (animatedObjects.empty() || currentClip == nullptr || !isPlaying)
        return;

    auto& mainClip = *currentClip;

    bool extractRootMotion = HasValidRootMotionRoot();
    RootMotionDelta mainRootMotion;
    if (extractRootMotion)
        mainRootMotion = ExtractRootMotionDelta(mainClip, mainClipTimePassed, deltaTime);

    UpdateAnimatedGameObject(
        mainClip,
        mainClipTimePassed,
        mainClipDurationInSeconds,
        mainClip.tickPerSecond,
        deltaTime,
        1.0f,
        extractRootMotion
    );

    if (blendClip)
    {
        RootMotionDelta blendRootMotion;
        if (extractRootMotion)
            blendRootMotion = ExtractRootMotionDelta(*blendClip, blendClipTimePassed, deltaTime);

        UpdateAnimatedGameObject(
            *blendClip,
            blendClipTimePassed,
            blendClipDurationInSeconds,
            blendClip->tickPerSecond,
            deltaTime,
            blendClipFactor,
            extractRootMotion
        );

        if (extractRootMotion)
            AccumulateRootMotionDelta(rootMotionDelta, BlendRootMotionDelta(mainRootMotion, blendRootMotion, blendClipFactor));
    }
    else if (extractRootMotion)
    {
        AccumulateRootMotionDelta(rootMotionDelta, mainRootMotion);
    }

    for (auto& a : animatedObjects)
    {
        a.go->SetLocalPosition(a.position);
        a.go->SetLocalScale(a.scale);
        a.go->SetLocalRotation(a.rotation);
    }
}

RootMotionDelta AnimationPlayer::ExtractRootMotionDelta(
    const Animation::AnimationClip& clip,
    float fromTime,
    float deltaTime
) const
{
    RootMotionDelta delta;
    if (!HasValidRootMotionRoot() || clip.duration <= 0.0f || clip.tickPerSecond <= 0.0f)
        return delta;

    float durationInSeconds = clip.duration / clip.tickPerSecond;
    if (durationInSeconds <= 0.0f)
        return delta;

    float scaledDeltaTime = deltaTime * speed;
    if (scaledDeltaTime == 0.0f)
        return delta;

    float remainingEndTime = fromTime + scaledDeltaTime;
    float segmentStart = fromTime;

    if (scaledDeltaTime > 0.0f)
    {
        while (remainingEndTime > durationInSeconds)
        {
            AccumulateRootMotionDelta(delta, ExtractRootMotionDeltaSegment(clip, segmentStart, durationInSeconds));
            remainingEndTime -= durationInSeconds;
            segmentStart = 0.0f;
        }
    }
    else
    {
        while (remainingEndTime < 0.0f)
        {
            AccumulateRootMotionDelta(delta, ExtractRootMotionDeltaSegment(clip, segmentStart, 0.0f));
            remainingEndTime += durationInSeconds;
            segmentStart = durationInSeconds;
        }
    }

    AccumulateRootMotionDelta(delta, ExtractRootMotionDeltaSegment(clip, segmentStart, remainingEndTime));
    delta.duration = std::abs(deltaTime);
    FinalizeRootMotionDelta(delta);
    return delta;
}

RootMotionDelta AnimationPlayer::ExtractRootMotionDeltaSegment(
    const Animation::AnimationClip& clip,
    float fromTime,
    float toTime
) const
{
    RootMotionDelta delta;
    if (!HasValidRootMotionRoot() || animatedRootGOIndex >= clip.channels.size())
        return delta;

    const Animation::Channel& channel = clip.channels[animatedRootGOIndex];
    float fromTick = fromTime * clip.tickPerSecond;
    float toTick = toTime * clip.tickPerSecond;

    glm::vec3 fromPosition = SamplePosition(channel, fromTick);
    glm::vec3 toPosition = SamplePosition(channel, toTick);
    glm::quat fromRotation = SampleRotation(channel, fromTick);
    glm::quat toRotation = SampleRotation(channel, toTick);

    delta.localTranslation = FilterTranslation(toPosition - fromPosition, rootMotionTranslationMode);
    delta.localRotation = FilterRotation(NormalizeSafe(glm::inverse(fromRotation) * toRotation), rootMotionRotationMode);
    delta.duration = std::abs(toTime - fromTime);
    FinalizeRootMotionDelta(delta);
    return delta;
}

RootMotionDelta AnimationPlayer::BlendRootMotionDelta(
    const RootMotionDelta& a,
    const RootMotionDelta& b,
    float blend
) const
{
    RootMotionDelta delta;
    float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
    delta.localTranslation = glm::mix(a.localTranslation, b.localTranslation, clampedBlend);
    delta.localRotation = NormalizeSafe(glm::slerp(a.localRotation, b.localRotation, clampedBlend));
    delta.duration = glm::mix(a.duration, b.duration, clampedBlend);
    FinalizeRootMotionDelta(delta);
    return delta;
}

void AnimationPlayer::AccumulateRootMotionDelta(RootMotionDelta& target, const RootMotionDelta& delta) const
{
    target.localTranslation += delta.localTranslation;
    target.localRotation = NormalizeSafe(target.localRotation * delta.localRotation);
    target.duration += delta.duration;
    FinalizeRootMotionDelta(target);
}

void AnimationPlayer::FinalizeRootMotionDelta(RootMotionDelta& delta) const
{
    delta.localRotation = NormalizeSafe(delta.localRotation);
    delta.hasTranslation = glm::dot(delta.localTranslation, delta.localTranslation) > RootMotionEpsilon;
    delta.hasRotation = std::abs(glm::dot(delta.localRotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))) <
                        1.0f - RootMotionEpsilon;

    if (gameObject)
    {
        glm::quat ownerRotation = gameObject->GetRotation();
        delta.translation = ownerRotation * delta.localTranslation;
        delta.rotation = NormalizeSafe(ownerRotation * delta.localRotation * glm::inverse(ownerRotation));
    }
    else
    {
        delta.translation = delta.localTranslation;
        delta.rotation = delta.localRotation;
    }
}

void AnimationPlayer::AdvanceClipTime(float& timePassed, float durationInSeconds, float deltaTime) const
{
    if (durationInSeconds <= 0.0f)
        return;

    timePassed += deltaTime * speed;
    while (timePassed > durationInSeconds)
        timePassed -= durationInSeconds;
    while (timePassed < 0.0f)
        timePassed += durationInSeconds;
}

RootMotionDelta AnimationPlayer::ConsumeRootMotionDelta()
{
    RootMotionDelta delta = rootMotionDelta;
    rootMotionDelta = RootMotionDelta{};
    return delta;
}

bool AnimationPlayer::SetClip(const std::string& animationName)
{
    isPlaying = false;
    this->initialActiveClip = animationName;

    bool success = SetClipInternal(animationName, currentClip, mainClipTimePassed, mainClipDurationInSeconds);

    if (success)
    {
        if (!SetupAnimatedObjects(*currentClip, GetGameObject()))
        {
            currentClip = nullptr;
            success = false;
        }
    }

    if (!rootName.empty())
    {
        SetRootMotionEnabled(true);
        EnableRootMotion();
    }
    else
    {
        SetRootMotionEnabled(false);
        animatedRootGOIndex = -1;
    }

    return success;
}

bool AnimationPlayer::SetupAnimatedObjects(const Animation::AnimationClip& clipUsed, GameObject* target)
{
    animatedObjects.resize(clipUsed.channels.size());

    int index = 0;
    for (auto& c : clipUsed.channels)
    {
        animatedObjects[index].go = target->Find(c.nodeName);
        if (animatedObjects[index].go == nullptr)
        {
            return false;
        }
        index++;
    }

    return true;
}

void AnimationPlayer::Stop()
{
    isPlaying = false;
    mainClipTimePassed = 0;
}

void AnimationPlayer::Play()
{
    isPlaying = true;
    mainClipTimePassed = 0;
}

void AnimationPlayer::Tick()
{
    if (!rootMotion)
        TickAnimation();
}

void AnimationPlayer::PrePhysicsAnimationTick()
{
    if (rootMotion && GetScene())
        TickAnimation(GetScene()->GetPhysicsScene().GetDeltaTime());
}

void AnimationPlayer::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("animation", animation);
    s->Serialize("speed", speed);
    s->Serialize("rootName", rootName);
    s->Serialize("rootMotionTranslationMode", static_cast<int32_t>(rootMotionTranslationMode));
    s->Serialize("rootMotionRotationMode", static_cast<int32_t>(rootMotionRotationMode));
    s->Serialize("activeClip", initialActiveClip);
    s->Serialize("blendClip", initialBlendClip);
}
void AnimationPlayer::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("animation", animation);
    s->Deserialize("speed", speed);
    s->Deserialize("rootName", rootName);
    int32_t translationMode = static_cast<int32_t>(rootMotionTranslationMode);
    int32_t rotationMode = static_cast<int32_t>(rootMotionRotationMode);
    s->Deserialize("rootMotionTranslationMode", translationMode);
    s->Deserialize("rootMotionRotationMode", rotationMode);
    rootMotionTranslationMode = static_cast<RootMotionTranslationMode>(translationMode);
    rootMotionRotationMode = static_cast<RootMotionRotationMode>(rotationMode);
    s->Deserialize("activeClip", initialActiveClip);
    s->Deserialize("blendClip", initialBlendClip);
}

void AnimationPlayer::OnLoaded()
{
    if (!initialActiveClip.empty())
        SetClip(initialActiveClip);

    if (!initialBlendClip.empty())
        SetBlendClip(initialBlendClip);
}

bool AnimationPlayer::SetBlendClip(const std::string& animationName)
{
    bool success = SetClipInternal(animationName, blendClip, blendClipTimePassed, blendClipDurationInSeconds);
    initialBlendClip = animationName;
    if (success)
    {
        success = IsBlendClipMatchWithMainClip();
        if (!success)
        {
            blendClip = nullptr;
        }
    }

    return success;
}

bool AnimationPlayer::SetClipInternal(
    const std::string& animationName,
    const Animation::AnimationClip*& clipToSet,
    float& timePassed,
    float& durationInSeconds
)
{
    if (animation == nullptr)
        return false;

    isPlaying = false;
    auto iter = animation->GetAnimationClips().find(animationName);
    if (iter != animation->GetAnimationClips().end())
    {
        clipToSet = iter->second.get();

        timePassed = 0;
        durationInSeconds = clipToSet->duration / clipToSet->tickPerSecond;
    }
    else
    {
        clipToSet = nullptr;
        return false;
    }

    mainClipTimePassed = 0;

    return true;
}

bool AnimationPlayer::IsBlendClipMatchWithMainClip()
{
    if (blendClip && currentClip)
    {
        if (currentClip->channels.size() != blendClip->channels.size())
            return false;

        for (int channelIndex = 0; channelIndex < currentClip->channels.size(); ++channelIndex)
        {
            if (blendClip->channels[channelIndex].nodeName != currentClip->channels[channelIndex].nodeName)
            {
                spdlog::error("blend clip and main clip don't match");
                return false;
            }
        }

        return true;
    }

    spdlog::error("current clip is not set");
    return false;
}

void AnimationPlayer::EnableRootMotion()
{
    animatedRootGOIndex = -1;
    if (rootName.empty())
        return;

    auto iter = std::find_if(
        animatedObjects.begin(),
        animatedObjects.end(),
        [this](AnimatedGameObject& go) { return go.go && go.go->GetName().compare(rootName) == 0; }
    );
    if (iter == animatedObjects.end())
    {
        spdlog::error("root motion set failed");
        return;
    }
    spdlog::info("root motion set success");
    animatedRootGOIndex = iter - animatedObjects.begin();
    return;
}

void AnimationPlayer::SetRootMotionRoot(std::string_view rootName)
{
    this->rootName = std::string(rootName);
    if (this->rootName.empty())
    {
        SetRootMotionEnabled(false);
        animatedRootGOIndex = -1;
        rootMotionDelta = RootMotionDelta{};
        return;
    }

    SetRootMotionEnabled(true);
    if (!animatedObjects.empty())
        EnableRootMotion();
}

void AnimationPlayer::SetRoot(std::string_view rootName)
{
    SetRootMotionRoot(rootName);
}

void AnimationPlayer::OnStart()
{
    if (!initialActiveClip.empty())
    {
        SetClip(initialActiveClip);
    }

    if (!initialBlendClip.empty())
    {
        SetBlendClip(initialBlendClip);
    }

    if (!rootName.empty())
    {
        EnableRootMotion();
    }

    if (!initialActiveClip.empty())
    {
        Play();
    }
}
