#pragma once
#include "Component.hpp"
#include "Core/Ptr.hpp"
#include "Rendering/Animation.hpp"

class AnimationPlayer : public Component
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
    const std::string& GetName() override;
    void Tick() override;
    void OnLoaded() override;

public:
    void SetSpeed(float speed) { this->speed = speed; }
    void SetBlendClipFactor(float blendClipFactor) { this->blendClipFactor = blendClipFactor; }
    bool SetBlendClip(const std::string& animationName);
    bool SetClip(const std::string& animationName);
    void SetRootMotionEnabled(bool enabled) { this->rootMotion = enabled; }
    void SetRoot(std::string_view rootName);

    const std::string& GetRootName() { return rootName; }
    bool IsRootMotionEnabled() { return rootMotion; }
    float GetSpeed() const { return speed; }
    float GetBlendClipFactor() const { return blendClipFactor; }
    void SetAnimation(Animation* animation) { this->animation = animation; }
    Animation* GetAnimation() { return animation; }
    const Animation::AnimationClip* GetActiveClip() const { return currentClip; }
    const Animation::AnimationClip* GetBlendClip() const { return blendClip; }
    const glm::vec3& GetRootMotionDelta() const { return rootMotionDelta; }
    void Play();
    void Stop();
    void TickAnimation();

private:
    // ***** Serialized ****** //
    ObjPtr<Animation> animation = nullptr;
    float speed = 1.0f;

    // ***** Runtime ******//
    bool isPlaying = false;
    const Animation::AnimationClip* currentClip = nullptr;
    const Animation::AnimationClip* blendClip = nullptr;
    struct AnimatedGameObject
    {
        GameObject* go = nullptr;
        glm::vec3 position = {0, 0, 0};
        glm::vec3 scale{1, 1, 1};
        glm::quat rotation{1, 0, 0, 0};
    };
    DynamicArray<AnimatedGameObject> animatedObjects;
    float mainClipTimePassed = 0;
    float mainClipDurationInSeconds = 0;
    float blendClipTimePassed = 0;
    float blendClipDurationInSeconds = 0;
    float blendClipFactor = 1.0f;
    bool rootMotion = false;
    int animatedRootGOIndex;
    std::string rootName = "";
    std::string initialActiveClip = "";
    std::string initialBlendClip = "";
    glm::vec3 rootMotionPreviousPosition = glm::vec3(0);
    glm::vec3 rootMotionDelta = glm::vec3(0);

    bool SetupAnimatedObjects(const Animation::AnimationClip& clipUsed, GameObject* target);
    void Copy(const AnimationPlayer& other) { animation = other.animation; }
    void UpdateAnimatedGameObject(
        const Animation::AnimationClip& mainClip,
        float& timePassed,
        float& durationInSeconds,
        float tickPerSecond,
        float blend
    );
    bool SetClipInternal(
        const std::string& animationName,
        const Animation::AnimationClip*& clipToSet,
        float& timePassed,
        float& durationInSeconds
    );
    bool IsBlendClipMatchWithMainClip();
    void EnableRootMotion();
};
