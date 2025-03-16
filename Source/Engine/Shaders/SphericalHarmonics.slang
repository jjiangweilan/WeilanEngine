import Library.Random;
struct Input
{
    TextureCube src;
    SamplerState srcSampler;
    float levels;

    RWBuffer<float> redCoefficients;
    RWBuffer<float> greenCoefficients;
    RWBuffer<float> blueCoefficients;

    Buffer<float> sampleDirections;
};

ParameterBlock<Input> input;

// Constants
#define PI 3.14159265359
#define NUM_SAMPLES 100

// Spherical Harmonics coefficients
static float3 SH[9];

// Function to sample the cubemap
float3 SampleCubemap(float3 direction);

// Function to compute the SH basis functions
float SHBasis(int l, int m, float3 dir);

// Function to generate a random direction
float3 RandomDirection(int sampleIdx)
{
    int offset = sampleIdx * 3;
    return float3(input.sampleDirections[offset], input.sampleDirections[offset+1], input.sampleDirections[offset+2]);
    //float z = 2.0 * rand() - 1.0;
    //float t = 2.0 * PI * rand();
    //float r = sqrt(1.0 - z * z);
    //return float3(r * cos(t), r * sin(t), z);
}

// Main function to compute SH coefficients from a cubemap
void ComputeSHFromCubemap(TextureCube cubemap)
{
    // Initialize SH coefficients to zero
    for (int i = 0; i < 9; ++i)
    {
        SH[i] = float3(0.0, 0.0, 0.0);
    }

    // Loop over samples
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        // Generate a random direction
        float3 dir = RandomDirection(i);

        // Sample the cubemap
        float3 color = input.src.SampleLevel(input.srcSampler, dir, 0).xyz;

        // Compute SH basis functions and accumulate SH coefficients
        for (int l = 0; l <= 2; ++l)
        {
            for (int m = -l; m <= l; ++m)
            {
                int index = l * (l + 1) + m;
                float basis = SHBasis(l, m, dir);
                SH[index] += color * basis;
            }
        }
    }

    // Normalize SH coefficients
    float weight = 4.0 * PI / NUM_SAMPLES;
    for (int i = 0; i < 9; ++i)
    {
        SH[i] *= weight;
    }
}

// Function to compute the SH basis functions
float SHBasis(int l, int m, float3 dir)
{
    if (l == 0 && m == 0)
    {
        return 0.282095; // Y00
    }
    else if (l == 1 && m == -1)
    {
        return 0.488603 * dir.y; // Y1-1
    }
    else if (l == 1 && m == 0)
    {
        return 0.488603 * dir.z; // Y10
    }
    else if (l == 1 && m == 1)
    {
        return 0.488603 * dir.x; // Y11
    }
    else if (l == 2 && m == -2)
    {
        return 1.092548 * dir.x * dir.y; // Y2-2
    }
    else if (l == 2 && m == -1)
    {
        return 1.092548 * dir.y * dir.z; // Y2-1
    }
    else if (l == 2 && m == 0)
    {
        return 0.315392 * (3.0 * dir.z * dir.z - 1.0); // Y20
    }
    else if (l == 2 && m == 1)
    {
        return 1.092548 * dir.x * dir.z; // Y21
    }
    else if (l == 2 && m == 2)
    {
        return 0.546274 * (dir.x * dir.x - dir.y * dir.y); // Y22
    }
    return 0.0;
}

[shader("compute")]
[numthreads(64,1,1)]
void Main()
{

}
