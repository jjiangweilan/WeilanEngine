struct Param
{
    RWTexture2D<float2> tex;
};

ParameterBlock<Param> param;

#define diff3(f, x, y) (((f((x) + (y))) + (f((x) - (y))) - 2.*(f((x)))) / dot(y,y))

float hash21(float2 p)
{
	float3 q = fract(float3(p.xyx) * .1031);
    q += dot(q, q.yzx + 19.19);
    return frac((q.x + q.y) * q.z);
}

//source https://www.shadertoy.com/view/Wtfczs
float bluenoise(float2 p)
{
    return 1./4. * (
          diff3(hash21, p, float2(1,0)) 
        + diff3(hash21, p, float2(0,1))
        ) + .5;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void Main(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    param.tex.GetDimensions(width, height);
    if (any(id.xy >= uint2(width, height)))
        return;

    // Generate two channels of pseudo-blue noise
    float r = bluenoise(id.xy);
    float g = bluenoise(id.xy + 1337); // Use an offset for the second channel

    param.tex[id.xy] = float2(r, g);
}
