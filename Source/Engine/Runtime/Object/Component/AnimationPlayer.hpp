#pragma once
#include "Component.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/System/Rendering/AnimationClip.hpp"
#include "Engine/Runtime/System/Rendering/AnimationSet.hpp"

enum class [[LuaEnum]] RootMotionTranslationMode
{
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Full = 3
};

enum class [[LuaEnum]] RootMotionRotationMode
{
    None = 0,
    Yaw = 1,
    Full = 2
};

struct [[LuaClass]] RootMotionDelta
{
    [[LuaProp]] glm::vec3 translation = glm::vec3(0.0f);
    [[LuaProp]] glm::vec3 localTranslation = glm::vec3(0.0f);
    [[LuaProp]] glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    [[LuaProp]] glm::quat localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    [[LuaProp]] float duration = 0.0f;
    [[LuaProp]] bool hasTranslation = false;
    [[LuaProp]] bool hasRotation = false;

    [[LuaFn]] bool IsEmpty() const { return !hasTranslation && !hasRotation; }
};

class [[LuaClass]] AnimationPlayer : public Component
{
    DECLARE_OBJECT();

public:
    AnimationPlayer();
    AnimationPlayer(GameObject* gameObject);
    AnimationPlayer(const AnimationPlayer& other) : Component(other) { Copy(other); }
    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        auto copy = std::make_unique<AnimationPlayer>(*this);
        return copy;
    }
    void OnStart() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() const override;
    void Tick() override;
    void OnLoaded() override;

public:
    void SetSpeed(float speed) { this->speed = speed; }
    void SetAutoPlay(bool autoPlay) { this->autoPlay = autoPlay; }
    [[LuaFn]] void SetBlendClipFactor(float blendClipFactor) { this->blendClipFactor = blendClipFactor; }
    [[LuaFn]] bool SetBlendClip(const std::string& animationName);
    [[LuaFn]]
    void ClearBlendClip();
    [[LuaFn]]
    bool SetClip(const std::string& animationName);
    void SetRootMotionEnabled(bool enabled) { this->rootMotion = enabled; }
    void SetRootMotionRoot(std::string_view rootName);
    void SetRootMotionTranslationMode(RootMotionTranslationMode mode) { rootMotionTranslationMode = mode; }
    void SetRootMotionRotationMode(RootMotionRotationMode mode) { rootMotionRotationMode = mode; }
    [[LuaNamedFn("SetRootMotionEnabled")]] void Lua_SetRootMotionEnabled(bool enabled) { SetRootMotionEnabled(enabled); }
    [[LuaNamedFn("SetRootMotionRoot")]] void Lua_SetRootMotionRoot(std::string rootName) { SetRootMotionRoot(rootName); }
    [[LuaNamedFn("SetRootMotionTranslationMode")]] void Lua_SetRootMotionTranslationMode(int mode)
    {
        SetRootMotionTranslationMode(static_cast<RootMotionTranslationMode>(mode));
    }
    [[LuaNamedFn("SetRootMotionRotationMode")]] void Lua_SetRootMotionRotationMode(int mode)
    {
        SetRootMotionRotationMode(static_cast<RootMotionRotationMode>(mode));
    }
    void SetRoot(std::string_view rootName);

    const std::string& GetRootName() { return rootName; }
    bool IsRootMotionEnabled() { return rootMotion; }
    RootMotionTranslationMode GetRootMotionTranslationMode() const { return rootMotionTranslationMode; }
    RootMotionRotationMode GetRootMotionRotationMode() const { return rootMotionRotationMode; }
    [[LuaNamedFn("GetRootMotionTranslationMode")]] int Lua_GetRootMotionTranslationMode() const
    {
        return static_cast<int>(rootMotionTranslationMode);
    }
    [[LuaNamedFn("GetRootMotionRotationMode")]] int Lua_GetRootMotionRotationMode() const
    {
        return static_cast<int>(rootMotionRotationMode);
    }
    float GetSpeed() const { return speed; }
    bool IsAutoPlay() const { return autoPlay; }
    float GetBlendClipFactor() const { return blendClipFactor; }
    bool IsPlaying() const { return isPlaying; }
    float GetMainClipTimePassed() const { return mainClipTimePassed; }
    float GetMainClipDurationInSeconds() const { return mainClipDurationInSeconds; }
    float GetBlendClipTimePassed() const { return blendClipTimePassed; }
    float GetBlendClipDurationInSeconds() const { return blendClipDurationInSeconds; }
    void SetAnimationSet(AnimationSet* animationSet) { this->animationSet = animationSet; }
    AnimationSet* GetAnimationSet() { return animationSet; }
    const AnimationClip* GetActiveClip() const { return currentClip; }
    const AnimationClip* GetBlendClip() const { return blendClip; }
    const RootMotionDelta& GetRootMotionDelta() const { return rootMotionDelta; }
    [[LuaFn]] RootMotionDelta PeekRootMotionDelta() const { return rootMotionDelta; }
    [[LuaFn]] RootMotionDelta ConsumeRootMotionDelta();
    [[LuaFn]] void Play();
    [[LuaFn]] void Stop();
    [[LuaFn]]
    void ResetBoneTransform();
    void TickAnimation();
    void TickAnimation(float deltaTime);
    void PrePhysicsAnimationTick() override;
    void IdleTick() override;

private:
    // ***** Serialized ****** //
    ObjPtr<AnimationSet> animationSet = nullptr;
    float speed = 1.0f;
    bool autoPlay = true;

    // ***** Runtime ******//
    bool isPlaying = false;
    const AnimationClip* currentClip = nullptr;
    const AnimationClip* blendClip = nullptr;
    struct AnimatedGameObject
    {
        GameObject* go = nullptr;
        glm::vec3 position = {0, 0, 0};
        glm::vec3 scale{1, 1, 1};
        glm::quat rotation{1, 0, 0, 0};
        glm::vec3 originalPosition = {0, 0, 0};
        glm::vec3 originalScale{1, 1, 1};
        glm::quat originalRotation{1, 0, 0, 0};
    };
    std::vector<AnimatedGameObject> animatedObjects;
    float mainClipTimePassed = 0;
    float mainClipDurationInSeconds = 0;
    float blendClipTimePassed = 0;
    float blendClipDurationInSeconds = 0;
    float blendClipFactor = 0.0f;
    bool rootMotion = false;
    int animatedRootGOIndex = -1;
    RootMotionTranslationMode rootMotionTranslationMode = RootMotionTranslationMode::Horizontal;
    RootMotionRotationMode rootMotionRotationMode = RootMotionRotationMode::None;
    std::string rootName = "";
    std::string initialActiveClip = "";
    std::string initialBlendClip = "";
    RootMotionDelta rootMotionDelta;

    bool SetupAnimatedObjects(const AnimationClip& clipUsed, GameObject* target);
    void Copy(const AnimationPlayer& other)
    {
        animationSet = other.animationSet;
        speed = other.speed;
        autoPlay = other.autoPlay;
        rootName = other.rootName;
        initialActiveClip = other.initialActiveClip;
        initialBlendClip = other.initialBlendClip;
        rootMotionTranslationMode = other.rootMotionTranslationMode;
        rootMotionRotationMode = other.rootMotionRotationMode;
    }
    void UpdateAnimatedGameObject(
        const AnimationClip& mainClip,
        float& timePassed,
        float& durationInSeconds,
        float tickPerSecond,
        float deltaTime,
        float blend,
        bool stripRootMotion
    );
    RootMotionDelta ExtractRootMotionDelta(const AnimationClip& clip, float fromTime, float deltaTime) const;
    RootMotionDelta ExtractRootMotionDeltaSegment(const AnimationClip& clip, float fromTime, float toTime) const;
    RootMotionDelta BlendRootMotionDelta(const RootMotionDelta& a, const RootMotionDelta& b, float blend) const;
    void AccumulateRootMotionDelta(RootMotionDelta& target, const RootMotionDelta& delta) const;
    void FinalizeRootMotionDelta(RootMotionDelta& delta) const;
    void AdvanceClipTime(float& timePassed, float durationInSeconds, float deltaTime) const;
    bool SetClipInternal(
        const std::string& animationName,
        const AnimationClip*& clipToSet,
        float& timePassed,
        float& durationInSeconds
    );
    bool IsBlendClipMatchWithMainClip();
    void EnableRootMotion();
    bool HasValidRootMotionRoot() const { return rootMotion && animatedRootGOIndex >= 0; }
};
