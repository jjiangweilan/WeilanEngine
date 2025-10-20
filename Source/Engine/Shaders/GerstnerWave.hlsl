#pragma once

// Tanget and Binornal with cross for Normal later
// after accumulation, tanget and binormal can be calculated as follow
// normal = normalize(float3(-normal.x, 1 - normal.y, -normal.z));
// binormal = normalize(float3(1 - binormal.x, binormal.y, -binormal.z));
// tangent = normalize(float3(-tangent.x, tangent.y, 1 - tangent.z));
float3 GerstnerWave(Wave wave, float3 position, float time, float waveNumber, inout float3 normal)
{
    float2 d = wave.direction;
    float A = wave.amplitude;
    float W = 2 * M_PI / wave.wavelength;
    float WA = W * A;
    float Q = wave.steepness / (WA * waveNumber);
    float speed = wave.speed * sqrt(9.8 / W); // use deep water theory to calculate speed

    float phase = speed * W;
    float f = (W * dot(d, position.xz) + phase * time);
    float QA = Q * A;
    
    // Calculate the wave displacement
    float3 result = float3(0.0, 0.0, 0.0);
    result.x = QA * d.x * cos(f);
    result.z = QA * d.y * cos(f);
    result.y = A * sin(f);

    float s0 = sin(f);
    float c0 = cos(f);

    normal.x = d.x * WA * c0;
    normal.z = d.y * WA * c0;
    normal.y = Q * WA * s0;

    // Calculate binormal vector (derivative with respect to x)
    // binormal.x = Q * d.x * d.x * WA * s0;// -d.x * d.y * steepness * sin(f);
    // binormal.z = Q * d.x * d.y * WA * s0;
    // binormal.y = d.x * WA * c0;
    // 
    // Calculate tangent vector (derivative with respect to z)
    // tangent.x = Q * d.x * d.y * WA * s0;
    // tangent.z = Q * d.y * d.y * WA * s0;
    // tangent.y = d.y * WA * c0;

    return result;
}
