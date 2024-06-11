#include "PlayerController.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "Core/Time.hpp"
#include "Gameplay/Input.hpp"
#include "Jolt/Physics/Collision/CollisionCollectorImpl.h"
#include <spdlog/spdlog.h>
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/ShapeCast.h>

DEFINE_OBJECT(PlayerController, "A14D66B4-47AB-4703-BEAA-06EBD285034F");

PlayerController::PlayerController() : Component(nullptr) {}

PlayerController::PlayerController(GameObject* gameObject) : Component(gameObject) {}

const std::string& PlayerController::GetName()
{
    static std::string name = "PlayerController";
    return name;
}

std::unique_ptr<Component> PlayerController::Clone(GameObject& owner)
{
    std::unique_ptr<PlayerController> clone = std::make_unique<PlayerController>(&owner);
    clone->SetCamera(target);
    clone->movementSpeed = movementSpeed;

    return clone;
}
void PlayerController::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("target", target);
    s->Serialize("movementSpeed", movementSpeed);
    s->Serialize("cameraDistance", cameraDistance);
    s->Serialize("rotateSpeed", rotateSpeed);
    s->Serialize("jumpImpulse", jumpImpulse);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("target", target);
    s->Deserialize("movementSpeed", movementSpeed);
    s->Deserialize("cameraDistance", cameraDistance);
    s->Deserialize("rotateSpeed", rotateSpeed);
    s->Deserialize("jumpImpulse", jumpImpulse);
}

void PlayerController::Tick()
{
    if (valid)
    {
        auto cameraGO = target->GetGameObject();
        if (cameraGO == GetGameObject())
            return;

        // set movement
        float mx, my;
        Input::GetSingleton().GetMovement(mx, my);
        mx *= movementSpeed * Time::DeltaTime();
        my *= movementSpeed * Time::DeltaTime();

        auto forward = cameraGO->GetForward();
        auto right = cameraGO->GetRight();

        // mute y
        forward.y = 0;
        right.y = 0;
        forward = glm::normalize(forward);
        right = glm::normalize(right);

        forward *= my;
        right *= mx;
        glm::vec3 velocity = forward + right;

        // jump
        if (Input::GetSingleton().Jump() && IsOnGround())
        {
            pbody->AddImpulse({0, jumpImpulse, 0});
        }

        // don't lose vertical speed
        auto v = pbody->GetLinearVelocity();
        velocity.y = v.y;
        pbody->SetLinearVelocity(velocity);

        // set camera lookat (camera position)
        auto characterPos = GetGameObject()->GetPosition();
        float lx, ly;
        Input::GetSingleton().GetLookAround(lx, ly);
        SetCameraSphericalPos(-lx, -ly);

        // set camera lookat character
        glm::vec3 cameraPos = cameraGO->GetPosition();
        auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
        cameraGO->SetRotation(lookAtQuat);
    }
}

void PlayerController::Awake()
{
    if (target == nullptr)
    {
        valid = false;
        return;
    }

    auto targetGO = target->GetGameObject();
    if (targetGO == GetGameObject())
    {
        valid = false;
        return;
    }

    // set camera's initial position
    SetCameraSphericalPos(0, 0);

    // set camera's initial rotation
    auto characterPos = GetGameObject()->GetPosition();
    glm::vec3 cameraPos = targetGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    targetGO->SetRotation(lookAtQuat);

    // get PhysicsBody
    auto bodies = GetGameObject()->GetComponentsInChildren<PhysicsBody>();
    if (!bodies.empty())
    {
        pbody = bodies[0];
        pbody->SetMotionType(JPH::EMotionType::Dynamic);

        // register contact event
        pbody->RegisterContactAddedEvent([this](
                                             PhysicsBody* self,
                                             PhysicsBody* other,
                                             const JPH::ContactManifold& manifold,
                                             JPH::ContactSettings& settings
                                         ) { ContactAddedEventCallback(self, other, manifold, settings); });
        pbody->RegisterContactRemovedEvent([this](PhysicsBody* self, PhysicsBody* other)
                                           { ContactRemovedEventCallback(self, other); });

        valid = true;
    }
    else
        valid = false;
}

void PlayerController::SetCameraSphericalPos(float xDelta, float yDelta)
{
    float rotSpeed = Time::DeltaTime() * rotateSpeed;
    phi += xDelta * rotSpeed;
    theta += yDelta * rotSpeed;
    glm::vec3 sphPos = {0, 0, 0};
    float cosTheta = glm::cos(theta);
    sphPos.x = glm::cos(phi) * cosTheta;
    sphPos.z = glm::sin(phi) * cosTheta;
    sphPos.y = sin(theta);
    glm::vec3 finalSphOffset = sphPos * cameraDistance;
    target->GetGameObject()->SetPosition(GetGameObject()->GetPosition() + finalSphOffset);
}

void PlayerController::ContactAddedEventCallback(
    PhysicsBody* self, PhysicsBody* other, const JPH::ContactManifold& manifold, JPH::ContactSettings& setting
)
{
    bool previousContacting = self->GetPhysicsScene()->GetPhysicsSystem().WereBodiesInContact(
        self->GetBody()->GetID(),
        other->GetBody()->GetID()
    );
    auto n = manifold.mWorldSpaceNormal;
    bool groundTest =
        glm::dot(glm::vec3(0, -1, 0), glm::vec3{n.GetX(), n.GetY(), n.GetZ()}) >= glm::cos(glm::radians(30.0f));
    if (groundTest)
    {
        isOnGround = !previousContacting;
    }
    spdlog::info("added, {}", previousContacting);
}

void PlayerController::ContactRemovedEventCallback(PhysicsBody* self, PhysicsBody* other)
{
    bool previousContacting = self->GetPhysicsScene()->GetPhysicsSystem().WereBodiesInContact(
        self->GetBody()->GetID(),
        other->GetBody()->GetID()
    );
    spdlog::info("removed, {}", previousContacting);
}

bool PlayerController::IsOnGround()
{
    JPH::RShapeCast cast{
        pbody->GetShapeRef().GetPtr(),
        JPH::Vec3::sReplicate(1.0f),
        JPH::RMat44::sTranslation(pbody->GetBody()->GetCenterOfMassPosition()),
        {0, -1, 0},
        pbody->GetBody()->GetWorldSpaceBounds()};
    JPH::ShapeCastSettings settings;
    JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;

    pbody->GetPhysicsScene()->GetPhysicsSystem().GetNarrowPhaseQuery().CastShape(
        cast,
        settings,
        JPH::Vec3::sReplicate(0.0f),
        collector
    );

    return collector.HadHit();
}
