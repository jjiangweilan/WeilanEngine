#pragma once
#include "Component.hpp"
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
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void Tick() override;

public:
    bool SetClip(const std::string& animationName);
    void Play();
    void Stop();
    void SetAnimation(Animation* animation) { this->animation = animation; }
    Animation* GetAnimation() { return animation; }
    const Animation::AnimationClip* GetActiveClip() const { return currentClip; }

    void TickAnimation();

private:
    // ***** Serialized ****** //
    Animation* animation = nullptr;

    // ***** Runtime ******//
    bool isPlaying = false;
    const Animation::AnimationClip* currentClip = nullptr;
    std::vector<GameObject*> animatedObjects;
    float timePassed = 0;

    bool SetupAnimatedObjects(const Animation::AnimationClip& clipUsed, GameObject* target);
    void Copy(const AnimationPlayer& other) { animation = other.animation; }
};
