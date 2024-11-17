#pragma once
#include "Component.hpp"
#include "Rendering/Animation.hpp"

class AnimationPlayer : public Component
{
    DECLARE_OBJECT();
public:

    AnimationPlayer();
    AnimationPlayer(GameObject* gameObject);
    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void Tick() override;

public:
    bool SetClip(const std::string& animationName, int startFrame, int endFrame);
    void Play();
    void Stop();
    void SetAnimation(Animation* animation) { this->animation = animation; }
    Animation* GetAnimation() { return animation; }
    const Animation::AnimationClip* GetActiveClip() { return currentClip; }

    void TickAnimation();

private:
    // ***** Serialized ****** //
    Animation* animation = nullptr;

    // ***** Runtime ******//
    bool isPlaying = false;
    const Animation::AnimationClip* currentClip = nullptr;
    std::vector<GameObject*> animatedObjects;
    double timePassed = 0;
    int currentStartFrame = 0;
    int currentEndFrame = -1;

    bool SetupAnimatedObjects(const Animation::AnimationClip& clipUsed, GameObject* target);
};
