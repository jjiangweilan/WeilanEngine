#pragma once

#include "Engine/Library/Math.hpp"

#include <span>
#include <vector>

namespace Algorithms
{
struct Boid
{
    float3 position = float3(0.0f);
    float3 velocity = float3(0.0f);
};

struct BoidsSettings
{
    float separationRadius = 1.0f;
    float alignmentRadius = 4.0f;
    float cohesionRadius = 4.0f;

    float separationWeight = 1.5f;
    float alignmentWeight = 1.0f;
    float cohesionWeight = 1.0f;

    float maxSpeed = 6.0f;
    float maxForce = 10.0f;
    float deltaTime = 1.0f / 60.0f;
};

std::vector<Boid> StepBoids(std::span<const Boid> boids, const BoidsSettings& settings = {});
} // namespace Algorithms
