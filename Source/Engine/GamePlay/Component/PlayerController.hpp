#pragma once
#include "Core/Component/Component.hpp"

class Camera;
class PlayerController : public Component
{
    DECLARE_OBJECT();

public:
    PlayerController();
    PlayerController(GameObject* gameObject);
    ~PlayerController() override{};

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;

    void SetCamera(Camera* camera)
    {
        this->target = camera;
    }

    Camera* GetCamera()
    {
        return target;
    }

private:
    Camera* target = nullptr;
};
