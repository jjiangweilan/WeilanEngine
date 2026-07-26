#if CONFIG
name: PostProcess/Bloom
#endif

import WeilanEngine;

struct RenderData
{
    float4 texelSize; // x, y = 1/width, 1/height, z, w = width, height
    float4 params;    // x = threshold, y = intensity, z = knee, w = scatter
    float mode;       // 0 = Prefilter, 1 = Downsample, 2 = Upsample, 3 = Composite
}

struct Input
{
    ConstantBuffer<RenderData> buffer;

    Texture2D src;
    [format("rgba16f")]
    RWTexture2D<float4> dst;
    SamplerState sampler_linear_clamp;
};

[Global]
ParameterBlock<Input> input;

float4 SafeColor(float4 c)
{
    return clamp(c, 0.0, 65000.0); // Prevent Infs
}

float4 Prefilter(float4 c)
{
    float brightness = max(c.r, max(c.g, c.b));
    float soft = brightness - input.buffer.params.x + input.buffer.params.z;
    soft = clamp(soft, 0.0, 2.0 * input.buffer.params.z);
    soft = soft * soft / (4.0 * input.buffer.params.z + 0.00001);
    float contribution = max(soft, brightness - input.buffer.params.x);
    contribution /= max(brightness, 0.00001);
    return c * contribution;
}

float4 DownsampleBox13Tap(Texture2D tex, SamplerState s, float2 uv, float2 texelSize)
{
    float4 A = tex.SampleLevel(s, uv + texelSize * float2(-1.0, -1.0), 0);
    float4 B = tex.SampleLevel(s, uv + texelSize * float2( 0.0, -1.0), 0);
    float4 C = tex.SampleLevel(s, uv + texelSize * float2( 1.0, -1.0), 0);
    float4 D = tex.SampleLevel(s, uv + texelSize * float2(-0.5, -0.5), 0);
    float4 E = tex.SampleLevel(s, uv + texelSize * float2( 0.5, -0.5), 0);
    float4 F = tex.SampleLevel(s, uv + texelSize * float2(-1.0,  0.0), 0);
    float4 G = tex.SampleLevel(s, uv                               , 0);
    float4 H = tex.SampleLevel(s, uv + texelSize * float2( 1.0,  0.0), 0);
    float4 I = tex.SampleLevel(s, uv + texelSize * float2(-0.5,  0.5), 0);
    float4 J = tex.SampleLevel(s, uv + texelSize * float2( 0.5,  0.5), 0);
    float4 K = tex.SampleLevel(s, uv + texelSize * float2(-1.0,  1.0), 0);
    float4 L = tex.SampleLevel(s, uv + texelSize * float2( 0.0,  1.0), 0);
    float4 M = tex.SampleLevel(s, uv + texelSize * float2( 1.0,  1.0), 0);

    float2 div = (1.0 / 4.0) * float2(0.5, 0.125);

    float4 o = (D + E + I + J) * div.x;
    o += (A + B + G + F) * div.y;
    o += (B + C + H + G) * div.y;
    o += (F + G + L + K) * div.y;
    o += (G + H + M + L) * div.y;

    return clamp(o, 0, 3);
}

float4 UpsampleTent(Texture2D tex, SamplerState s, float2 uv, float2 texelSize, float4 sampleScale)
{
    float4 d = texelSize.xyxy * float4(1.0, 1.0, -1.0, 0.0) * sampleScale;

    float4 s1 = tex.SampleLevel(s, uv - d.xy, 0);
    float4 s2 = tex.SampleLevel(s, uv - d.wy, 0);
    float4 s3 = tex.SampleLevel(s, uv - d.zy, 0);

    float4 s4 = tex.SampleLevel(s, uv + d.zw, 0);
    float4 s5 = tex.SampleLevel(s, uv       , 0);
    float4 s6 = tex.SampleLevel(s, uv + d.xw, 0);

    float4 s7 = tex.SampleLevel(s, uv + d.zy, 0);
    float4 s8 = tex.SampleLevel(s, uv + d.wy, 0);
    float4 s9 = tex.SampleLevel(s, uv + d.xy, 0);

    return (s1 + s2 * 2.0 + s3 + s4 * 2.0 + s5 * 4.0 + s6 * 2.0 + s7 + s8 * 2.0 + s9) * (1.0 / 16.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= input.buffer.texelSize.z || dispatchThreadID.y >= input.buffer.texelSize.w)
        return;

    float2 uv = (float2(dispatchThreadID.xy) + 0.5) * input.buffer.texelSize.xy;
    
    float4 result = float4(0, 0, 0, 0);

    if (input.buffer.mode < 0.5) // Prefilter (0)
    {
        // Downsample + Threshold
        // Using a simple box filter or just bilinear sample from source? 
        // Typically prefilter is combined with first downsample.
        // We will sample 13-tap from source and apply threshold.
        // NOTE: input.src is the full resolution image.
        
        float4 color = DownsampleBox13Tap(input.src, input.sampler_linear_clamp, uv, input.buffer.texelSize.xy * 2.0); // *2.0 because we are writing to half res
        result = Prefilter(SafeColor(color));
    }
    else if (input.buffer.mode < 1.5) // Downsample (1)
    {
        result = DownsampleBox13Tap(input.src, input.sampler_linear_clamp, uv, input.buffer.texelSize.xy * 2.0); // Source is higher res
        result = SafeColor(result);
    }
    else if (input.buffer.mode < 2.5) // Upsample (2)
    {
        // dst = High Res (Target)
        // src = Low Res (Source)
        float4 highRes = input.dst[dispatchThreadID.xy];
        float4 lowRes = UpsampleTent(input.src, input.sampler_linear_clamp, uv, input.buffer.texelSize.xy, float4(1.0, 1.0, 1.0, 1.0)); // Source is lower res
        
        result = highRes + lowRes * input.buffer.params.w; // scatter
    }
    else // Composite (3)
    {
        // input.src = Bloom Texture (upsampled)
        // input.dst = Main Color (Target)
        
        float4 mainColor = input.dst[dispatchThreadID.xy];
        float4 bloom = UpsampleTent(input.src, input.sampler_linear_clamp, uv, input.buffer.texelSize.xy, float4(1.0, 1.0, 1.0, 1.0));
        
        result = mainColor + bloom * input.buffer.params.y; // intensity
    }

    input.dst[dispatchThreadID.xy] = result;
}
