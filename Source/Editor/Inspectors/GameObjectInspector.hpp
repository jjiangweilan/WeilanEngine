#pragma once

#include "Editor/EditorState.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/Object/Component/LightFieldProbes.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Game/Component/PlayerController.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Editor
{
class GameObjectInspector : public Inspector<GameObject>
{
public:
    void DrawInspector(GameEditor& editor) override;

private:
    void PreventNegativeZero(float& val);

private:
    static char _register;
    glm::vec3 point, axis;
    float angle;
    Component* contextComponent = nullptr;
    int contextComponentIdx = -1;
    std::string searchComponent = "";
};

} // namespace Editor
