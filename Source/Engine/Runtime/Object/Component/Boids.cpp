#include "Boids.hpp"

#include "Engine/Core/Time.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

DEFINE_COMPONENT(Boids, "B7E657BC-23B7-48B6-85A0-868EAA72E7C6")

void Boids::Tick()
{
    std::vector<GameObject*> children;
    children.reserve(gameObject->GetChildren().size());

    for (auto child : gameObject->GetChildren())
    {
        if (child != nullptr && child->IsActiveInScene())
        {
            children.push_back(child);
        }
    }

    if (children.empty())
    {
        velocities.clear();
        return;
    }

    const size_t previousVelocityCount = velocities.size();
    velocities.resize(children.size(), float3(0.0f));

    float3 initialVelocity = float3(0.0f);
    if (settings.maxSpeed > 0.0f && gameObject != nullptr)
    {
        initialVelocity = gameObject->GetForward() * settings.maxSpeed * 0.5f;
    }

    for (size_t i = previousVelocityCount; i < velocities.size(); ++i)
    {
        velocities[i] = initialVelocity;
    }

    std::vector<Algorithms::Boid> boids;
    boids.reserve(children.size());

    for (size_t i = 0; i < children.size(); ++i)
    {
        boids.push_back({.position = children[i]->GetPosition(), .velocity = velocities[i]});
    }

    Algorithms::BoidsSettings stepSettings = settings;
    stepSettings.deltaTime = Time::DeltaTime();

    std::vector<Algorithms::Boid> updatedBoids = Algorithms::StepBoids(boids, stepSettings);

    for (size_t i = 0; i < children.size(); ++i)
    {
        children[i]->SetPosition(updatedBoids[i].position);
        velocities[i] = updatedBoids[i].velocity;
    }
}

void Boids::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("separationRadius", settings.separationRadius);
    s->Serialize("alignmentRadius", settings.alignmentRadius);
    s->Serialize("cohesionRadius", settings.cohesionRadius);
    s->Serialize("separationWeight", settings.separationWeight);
    s->Serialize("alignmentWeight", settings.alignmentWeight);
    s->Serialize("cohesionWeight", settings.cohesionWeight);
    s->Serialize("maxSpeed", settings.maxSpeed);
    s->Serialize("maxForce", settings.maxForce);
}

void Boids::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("separationRadius", settings.separationRadius);
    s->Deserialize("alignmentRadius", settings.alignmentRadius);
    s->Deserialize("cohesionRadius", settings.cohesionRadius);
    s->Deserialize("separationWeight", settings.separationWeight);
    s->Deserialize("alignmentWeight", settings.alignmentWeight);
    s->Deserialize("cohesionWeight", settings.cohesionWeight);
    s->Deserialize("maxSpeed", settings.maxSpeed);
    s->Deserialize("maxForce", settings.maxForce);
}

std::unique_ptr<Component> Boids::Clone(GameObject& owner)
{
    std::unique_ptr<Boids> clone = std::make_unique<Boids>(&owner);
    clone->enabled = enabled;
    clone->settings = settings;
    return clone;
}
