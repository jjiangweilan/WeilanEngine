#pragma once
#include "Core/Component/Component.hpp"

class Camera;
class PlayerController : public Component
{
    DECLARE_OBJECT();

public:
    float movementSpeed = 1.0f;
    float rotateSpeed = 0.3f;
    float cameraDistance = 5.0f;

    PlayerController();
    PlayerController(GameObject* gameObject);
    ~PlayerController() override{};

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void Tick() override;

    void SetCamera(Camera* camera)
    {
        this->target = camera;
    }

    Camera* GetCamera()
    {
        return target;
    }

    void Awake() override;

private:
    Camera* target = nullptr;
    void SetCameraSphericalPos(float xDelta, float yDelta);

    void AutoRotate();

    // camera rotation around player
    float theta, phi;
};
