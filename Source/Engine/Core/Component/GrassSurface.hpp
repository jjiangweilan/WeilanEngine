#pragma once
#include "Component.hpp"

class GrassSurface : public Component
{

    DECLARE_OBJECT();

public:
    GrassSurface();
    GrassSurface(GameObject* gameObject);
    ~GrassSurface();
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    Material computeDispatchMat;
    Material drawMat;
    std::unique_ptr<Gfx::Buffer> grassDispatcherIndirectDrawBuffer;

    void Awake() override;

private:
    void Init();
    void EnableImple() override;
    void DisableImple() override;
};
