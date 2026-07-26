import Library.PerScene;

float3 ApplyTreeWind(
    float3 localPosition,
    float4x4 objectModelMatrix,
    GpuMaterial material,
    float time
)
{
    float3 worldPosition = mul(objectModelMatrix, float4(localPosition, 1.0f)).xyz;

    float2 windDirection = material.windDirectionStrengthSpeed.xy;
    float directionLength = length(windDirection);
    windDirection = directionLength > 0.0001f ? windDirection / directionLength : float2(1.0f, 0.0f);
    float2 crossDirection = float2(-windDirection.y, windDirection.x);

    float bendHeight = max(material.windFrequencyHeights.z, 0.001f);
    float heightWeight = saturate((localPosition.y - material.windFrequencyHeights.y) / bendHeight);
    heightWeight = heightWeight * heightWeight * (3.0f - 2.0f * heightWeight);

    float strength = material.windDirectionStrengthSpeed.z;
    float speed = material.windDirectionStrengthSpeed.w;
    float frequency = material.windFrequencyHeights.x;
    float primaryPhase = dot(worldPosition.xz, windDirection) * frequency + time * speed;
    float crossPhase = dot(worldPosition.xz, crossDirection) * frequency * 1.7f + time * speed * 1.9f;
    float2 displacement =
        windDirection * sin(primaryPhase) +
        crossDirection * sin(crossPhase) * 0.35f;

    worldPosition.xz += displacement * strength * heightWeight;
    return worldPosition;
}
