#pragma once
#include "Component.hpp"
#include "Rendering/Animation.hpp"

class AnimationPlayer : public Component
{
public:
    DECLARE_OBJECT();

    AnimationPlayer();
    AnimationPlayer(GameObject* gameObject);
    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    void Serialize(Serializer* s) const override {};
    void Deserialize(Serializer* s) override {};
    const std::string& GetName() override;

public:
    bool Initialize(std::vector<GameObject*> animatedObjects);
    void Stop();
    void SetAnimation(Animation* animation) { this->animation = animation; }
    Animation* GetAnimation() { return animation; }
    const Animation::AnimationClip* GetActiveClip() { return currentClip; }
    bool PlayAnimation(const std::string& animationName, int startFrame, int endFrame);

    void TickAnimation();

private:
    Animation* animation;
    const Animation::AnimationClip* currentClip = nullptr;
    std::vector<GameObject*> animatedObjects;
    double timePassed = 0;
    int currentStartFrame = 0;
    int currentEndFrame = -1;
};
