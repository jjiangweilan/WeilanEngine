#include "Boids.hpp"

#include <algorithm>

namespace
{
constexpr float epsilon = 0.000001f;

float LengthSq(const float3& v)
{
    return glm::dot(v, v);
}

float3 Limit(const float3& v, float maxLength)
{
    if (maxLength <= 0.0f)
        return float3(0.0f);

    float lengthSq = LengthSq(v);
    float maxLengthSq = maxLength * maxLength;
    if (lengthSq <= maxLengthSq || lengthSq <= epsilon)
        return v;

    return glm::normalize(v) * maxLength;
}

float3 SteerToward(const float3& desiredDirection, const float3& currentVelocity, const Algorithms::BoidsSettings& settings)
{
    if (LengthSq(desiredDirection) <= epsilon || settings.maxSpeed <= 0.0f)
        return float3(0.0f);

    float3 desiredVelocity = glm::normalize(desiredDirection) * settings.maxSpeed;
    return Limit(desiredVelocity - currentVelocity, settings.maxForce);
}
} // namespace

namespace Algorithms
{
std::vector<Boid> StepBoids(std::span<const Boid> boids, const BoidsSettings& settings)
{
    std::vector<Boid> result(boids.begin(), boids.end());

    if (boids.empty() || settings.deltaTime <= 0.0f)
        return result;

    const float separationRadiusSq = settings.separationRadius * settings.separationRadius;
    const float alignmentRadiusSq = settings.alignmentRadius * settings.alignmentRadius;
    const float cohesionRadiusSq = settings.cohesionRadius * settings.cohesionRadius;

    for (size_t i = 0; i < boids.size(); ++i)
    {
        const Boid& current = boids[i];

        float3 separation = float3(0.0f);
        float3 alignment = float3(0.0f);
        float3 cohesion = float3(0.0f);
        int separationCount = 0;
        int alignmentCount = 0;
        int cohesionCount = 0;

        for (size_t j = 0; j < boids.size(); ++j)
        {
            if (i == j)
                continue;

            const Boid& other = boids[j];
            const float3 offset = other.position - current.position;
            const float distanceSq = LengthSq(offset);

            if (distanceSq <= epsilon)
                continue;

            if (settings.separationRadius > 0.0f && distanceSq < separationRadiusSq)
            {
                separation -= offset / distanceSq;
                ++separationCount;
            }

            if (settings.alignmentRadius > 0.0f && distanceSq < alignmentRadiusSq)
            {
                alignment += other.velocity;
                ++alignmentCount;
            }

            if (settings.cohesionRadius > 0.0f && distanceSq < cohesionRadiusSq)
            {
                cohesion += other.position;
                ++cohesionCount;
            }
        }

        float3 steering = float3(0.0f);

        if (separationCount > 0)
        {
            steering += SteerToward(separation / static_cast<float>(separationCount), current.velocity, settings) *
                        settings.separationWeight;
        }

        if (alignmentCount > 0)
        {
            steering += SteerToward(alignment / static_cast<float>(alignmentCount), current.velocity, settings) *
                        settings.alignmentWeight;
        }

        if (cohesionCount > 0)
        {
            float3 cohesionCenter = cohesion / static_cast<float>(cohesionCount);
            steering += SteerToward(cohesionCenter - current.position, current.velocity, settings) * settings.cohesionWeight;
        }

        steering = Limit(steering, settings.maxForce);

        Boid& output = result[i];
        output.velocity = Limit(current.velocity + steering * settings.deltaTime, settings.maxSpeed);
        output.position = current.position + output.velocity * settings.deltaTime;
    }

    return result;
}
} // namespace Algorithms
