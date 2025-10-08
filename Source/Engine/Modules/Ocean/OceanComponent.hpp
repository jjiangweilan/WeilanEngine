#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "Rendering/Shader2.hpp"

class OceanComponent : public RenderingComponent<OceanComponent>
{
    DECLARE_RENDERING_COMPONENT(OceanComponent);
    DECLARE_SERIALIZATION()

    ObjPtr<Mesh> plane;
    ObjPtr<Shader2> oceanShader;

public:
    void OnInit() override;

private:
};
