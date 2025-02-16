
struct Param
{
[format("r8")]
RWTexture2D<float> tex;
};

ParameterBlock<Param> param;

float IGN(float pixelX, float pixelY) { return fmod(52.9829189f * fmod(0.06711056f*float(pixelX) + 0.00583715f*float(pixelY), 1.0f), 1.0f); }

[shader("compute")]
[numthreads(8,8,1)]
void Main(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    param.tex.GetDimensions(width, height);
    if (any(id.xy >= uint2(width, height)))
        return;
    param.tex[id.xy] = IGN(id.x, id.y);
}


