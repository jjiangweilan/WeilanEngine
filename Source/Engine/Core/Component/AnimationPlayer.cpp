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

void AnimationPlayer::UpdateAnimatedGameObject(
    const Animation::AnimationClip& mainClip,
    float& timePassed,
    float& durationInSeconds,
    float tickPerSecond,
    float blend
)
{
    size_t index;
    float a;

    float currentTime = timePassed * tickPerSecond;
    for (int channelIndex = 0; channelIndex < mainClip.channels.size(); ++channelIndex)
    {
        auto& channel = mainClip.channels[channelIndex];
        auto& bone = animatedObjects.at(channelIndex);
        // position

        index = 0;
        glm::vec3 newPosition = channel.positions.front().val;
        if (currentTime > channel.positions.front().time)
        {
            if (currentTime <= channel.positions.back().time)
            {
                while (currentTime > channel.positions[index].time)
                    index++;
                auto p1 = channel.positions[index - 1];
                auto p2 = channel.positions[index];
                float a = (currentTime - p1.time) / (p2.time - p1.time);
                newPosition = (1 - a) * p1.val + a * p2.val;
            }
            else
                newPosition = channel.positions.back().val;
        }
        if (rootMotion && channelIndex == animatedRootGOIndex)
        {
            glm::vec3 tmp = newPosition;
            tmp.x = 0;
            tmp.z = 0;
            rootMotionDelta = newPosition - tmp;
            newPosition = tmp;
        }
        bone.position = glm::mix(bone.position, newPosition, blend);

        index = 0;
        glm::quat newRotation = channel.rotations.front().val;
        if (currentTime > channel.rotations.front().time)
        {
            if (currentTime <= channel.rotations.back().time)
            {
                while (currentTime > channel.rotations[index].time)
                    index++;
                auto r1 = channel.rotations[index - 1];
                auto r2 = channel.rotations[index];
                a = (currentTime - r1.time) / (r2.time - r1.time);
                newRotation = glm::slerp(r1.val, r2.val, a); // slerp to ensure shortest path is taken
            }
            else
                newRotation = channel.rotations.back().val;
        }
        bone.rotation = glm::slerp(bone.rotation, newRotation, blend);

        index = 0;
        glm::vec3 newScale = channel.scalings.front().val;
        if (currentTime > channel.scalings.front().time)
        {
            if (currentTime <= channel.scalings.back().time)
            {
                while (currentTime > channel.scalings[index].time)
                    index++;
                auto s1 = channel.scalings[index - 1];
                auto s2 = channel.scalings[index];
                a = (currentTime - s1.time) / (s2.time - s1.time);
                newScale = (1 - a) * s1.val + a * s2.val;
            }
            else
                newScale = channel.scalings.back().val;
        }
        bone.scale = glm::mix(bone.scale, newScale, blend);
    }

    timePassed += Time::DeltaTime() * speed;
    if (timePassed > durationInSeconds)
        timePassed = 0;
}

void AnimationPlayer::TickAnimation()
{
    if (animatedObjects.empty() || currentClip == nullptr || !isPlaying)
        return;

    auto& mainClip = *currentClip;

    UpdateAnimatedGameObject(mainClip, mainClipTimePassed, mainClipDurationInSeconds, mainClip.tickPerSecond, 1.0f);

    if (blendClip)
    {
        UpdateAnimatedGameObject(
            *blendClip,
            blendClipTimePassed,
            blendClipDurationInSeconds,
            blendClip->tickPerSecond,
            blendClipFactor
        );
    }

    for (auto& a : animatedObjects)
    {
        a.go->SetLocalPosition(a.position);
        a.go->SetLocalScale(a.scale);
        a.go->SetLocalRotation(a.rotation);
    }
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
        SetRootMotionEnabled(true);
    else
        SetRootMotionEnabled(false);

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
    TickAnimation();
}

void AnimationPlayer::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("animation", animation);
    s->Serialize("speed", speed);
    s->Serialize("rootName", rootName);
    s->Serialize("activeClip", initialActiveClip);
}
void AnimationPlayer::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("animation", animation);
    s->Deserialize("speed", speed);
    s->Deserialize("rootName", rootName);
    s->Deserialize("activeClip", initialActiveClip);
}

bool AnimationPlayer::SetBlendClip(const std::string& animationName)
{
    bool success = SetClipInternal(animationName, blendClip, blendClipTimePassed, blendClipDurationInSeconds);

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
    auto iter = std::find_if(
        animatedObjects.begin(),
        animatedObjects.end(),
        [this](AnimatedGameObject& go) { return go.go->GetName().compare(rootName) == 0; }
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

void AnimationPlayer::SetRoot(std::string_view rootName)
{
    this->rootName = rootName;
}

void AnimationPlayer::OnStart()
{
    if (!initialActiveClip.empty())
    {
        SetClip(initialActiveClip);
    }

    if (!rootName.empty())
    {
        EnableRootMotion();
    }
}
