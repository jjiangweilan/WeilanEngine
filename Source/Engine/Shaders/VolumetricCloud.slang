import Library.PerScene;

#if CONFIG
name: Game/Cloud
cull: front
blend:
- one oneMinusSrcAlpha
#endif

ParameterBlock<PerScene> scene;

struct VertexOutput {
    float4 position : SV_Position;
    float3 positionWS;
};

struct Transform
{
    float4x4 model;
};
[vk::push_constant]
Transform transform;

struct VertexInput
{
    float3 position;
};

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;

    float3 positionWS = mul(transform.model, float4(input.position, 1)).xyz;
    output.positionWS = positionWS.xyz;
    output.position = mul(scene.viewProjection, float4(positionWS, 1));
}

struct Cloud
{
    float4 position;
    float delta;
    float lightStep;
    float phaseFactor;

   // float4 worley_freq;
   // float4 perlin0_freq;
   // float4 perlin1_freq;

    float cloudDensity;
    float absorption;
    float dualLobHenyeyGreenstein;
    float4 cloudDensity_remap;
    float2 edgeFade;
    float lightIntensityScale;
    float4 sdfScale;
    float4 lighting_remap;
    float4 worley_amp;
} prop;

// layout(set = SET_GLOBAL, binding = 7) uniform Global
// {
//     float time;
// } g;

struct PerMaterial
{
    Texture3D cloudDensity;
    SamplerState sampler_Linear_ClampToBorder;
}

#define M_PI 3.1415926
#define USE_PERLIN_NOISE_3D
#define USE_WORLEY_NOISE_3D
#include "Utils/Noise.hlsl"
#include "Utils/Util.hlsl"

ParameterBlock<PerMaterial> perMaterial;

float BoxSdf(float3 p, float scale)
{
  // // mat4 model = transform.model[3].xyz;
  float3 b = 0.5 * scale * float3(length(transform.model[0].xyz), length(transform.model[1].xyz), length(transform.model[2].xyz));
  float3 q = abs(p - transform.model[3].xyz) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
  // return -length(p - transform.model[3].xyz)+length(transform.model[0].xyz);
}

float BoxSdf(float3 p, float3 scale)
{
  // // mat4 model = transform.model[3].xyz;
  float3 b = 0.5 * scale * float3(length(transform.model[0].xyz), length(transform.model[1].xyz), length(transform.model[2].xyz));
  float3 q = abs(p - transform.model[3].xyz) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
  // return -length(p - transform.model[3].xyz)+length(transform.model[0].xyz);
}

float Absorption(float d)
{
    return 2 * exp(-1 * d * prop.absorption) * (1-exp(-d*2));
}

float HenyeyGreenstein(float g, float costh)
{
    return (1.0 - g * g) / (4.0 * M_PI * pow(1.0 + g * g - 2.0 * g * costh, 3.0/2.0));
}

float DualLobHenyeyGreenstein(float g, float costh, float K)
{
    return lerp(HenyeyGreenstein(-g, costh), HenyeyGreenstein(g, costh), K);
}

float3 NormalizePosition(float3 pos)
{
    // we use a 1m box
    float3 extend = 0.5 * float3(length(transform.model[0].xyz), length(transform.model[1].xyz), length(transform.model[2].xyz));
    float3 cubePos = transform.model[3].xyz;

    float3 o = (pos - cubePos + extend) / (2 * extend);
    return o;

}


float SampleCloudDensity(float3 pos)
{
    float4 cloud0 = perMaterial.cloudDensity.Sample(perMaterial.sampler_Linear_ClampToBorder, NormalizePosition(pos));

    float perlinWorley = cloud0.x;
    float worley = cloud0.y * .625 + cloud0.z * .125 + cloud0.w * .25; 

    float cloud = remap(perlinWorley, worley - 1., 1., 0., 1.);

    return clamp(remap(cloud * prop.cloudDensity, prop.cloudDensity_remap),0, 1);
}

float SampleCloudDensity(float3 pos, out float3 normalizedPos)
{
    normalizedPos = NormalizePosition(pos);
    return clamp(remap(perMaterial.cloudDensity.Sample(perMaterial.sampler_Linear_ClampToBorder, normalizedPos).x * prop.cloudDensity, prop.cloudDensity_remap.x, prop.cloudDensity_remap.y,prop.cloudDensity_remap.z,prop.cloudDensity_remap.w),0, 1);
}


float SampleCloudDensity1(float3 uv)
{
    return clamp(remap(perMaterial.cloudDensity.Sample(perMaterial.sampler_Linear_ClampToBorder, uv).x * prop.cloudDensity, prop.cloudDensity_remap.x, prop.cloudDensity_remap.y,prop.cloudDensity_remap.z,prop.cloudDensity_remap.w),0, 1);
}

float GetDensity(float3 curr)
{
    float sdf = BoxSdf(curr, prop.sdfScale.xyz);
    float3 normalizedPos = NormalizePosition(curr);

    float4 cloud0 = perMaterial.cloudDensity.Sample(perMaterial.sampler_Linear_ClampToBorder, normalizedPos);

    float perlinWorley = cloud0.x;
    float worley = cloud0.y * prop.worley_amp.x + cloud0.z * prop.worley_amp.y + cloud0.w * prop.worley_amp.z; 

    float cloud = remap(perlinWorley, worley - 1., 1., 0., 1.);

    return remap(cloud - max(sdf, 0)* prop.edgeFade.x, prop.cloudDensity_remap);
}

float3 LightMarch(float3 end, float3 ldir)
{
    const float travelDistance = 20;
    const float delta = prop.lightStep > 0 ? prop.lightStep : 1;

    float travel = 0;
    float3 curr = end - travelDistance * ldir;
    float3 lightColor = float3(prop.lightIntensityScale);
    float4 light = float4(0);
    for(;travel < travelDistance; travel += delta)
    {
        float cloudDensity = GetDensity(curr);

        if (cloudDensity > 0)
        {
            float den = cloudDensity;
            // float cloudDensity0 = GetDensity(curr + 0.3 * ldir);
            // float den1 = SampleCloudDensity(curr + 0.1 * ldir);

            // float ndotl = clamp(cloudDensity - cloudDensity0, 0, 1);
            float phaseCos = 1;
            float phase = DualLobHenyeyGreenstein(prop.phaseFactor, phaseCos, prop.dualLobHenyeyGreenstein);
            float reflectAbsorption = Absorption(den);
            // float ndotl = clamp((den - den1) / 0.3,0,1);

            float4 col = float4(lightColor.xyz, den);


            col.rgb *= (col.a * phase * reflectAbsorption );
            light += col * (1-light.a);
        }
        // float val = normalizedPos.y*2-1;
        // float heightDensity = clamp(-pow(abs(val), 6.0f) + 1,0, 1);
        // cloudDensity *= heightDensity;

        curr += ldir * delta;
    }

    return light.xyz;//Absorption(distance);
}

float4 RayMarching1(float3 origin, float3 dir)
{
    const float travelDistance = 20;
    const float delta = prop.delta > 0 ? prop.delta : 0.01f;

    float travel = 0.01;
    float3 curr = origin;
    float distance = 0;
    float3 ldir = normalize(scene.lights[0].position.xyz);
    float4 light = float4(0);
    for(;travel < travelDistance; travel += delta)
    {
        float cloudDensity = GetDensity(curr);

        if (cloudDensity > 0)
        {
            float den = cloudDensity;
            float cloudDensity0 = GetDensity(curr + 0.1 * ldir);

            float ndotl = clamp(cloudDensity - cloudDensity0, 0, 1);
            float3 l = float3(float3(0.93,0.85,0.75)*0.9 + prop.lightIntensityScale*float3(1.0,0.6,0.3)*ndotl);

            float phaseCos = dot(-dir, -ldir);
            // float phase = DualLobHenyeyGreenstein(prop.phaseFactor, phaseCos, prop.dualLobHenyeyGreenstein);
            float absorption = Absorption(den);

            // float3 lightColor = LightMarch(curr, ldir);
            float4 col = float4(lerp( float3(1.0,0.93,0.84), float3(0.25,0.3,0.4), den), min(den * absorption, 1));
            col.rgb *= l * (col.a * absorption);// * phase );
            light += col * (1-light.a);
        }
        // float val = normalizedPos.y*2-1;
        // float heightDensity = clamp(-pow(abs(val), 6.0f) + 1,0, 1);
        // cloudDensity *= heightDensity;

        curr += dir * delta;
    }

    return light;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
float len = length(input.positionWS.xyz - scene.viewPos.xyz);
    float3 dir = (input.positionWS.xyz - scene.viewPos.xyz) / len;
    float3 end = input.positionWS.xyz;
    const float travelDistance = min(20, len);

    float4 lighting = RayMarching1(end - travelDistance * dir, dir);
    float4 result = float4(remap(lighting.xyz, prop.lighting_remap), clamp(lighting.a, 0, 1));//clamp(directAbsorption,0,1));
    return result;
}
