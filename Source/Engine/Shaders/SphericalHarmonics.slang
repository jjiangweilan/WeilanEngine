import Library.Random;

#define PI 3.14159265359
#define NUM_SAMPLES 100

float3 EvaluateSH(float3 normal, float4 shCoefficients[9])
{
    // SH basis functions for second-order
    float x = normal.x;
    float y = normal.y;
    float z = normal.z;

    float shBasis[9];
    shBasis[0] = 0.282095f; // Y00
    shBasis[1] = 0.488603f * y; // Y1-1
    shBasis[2] = 0.488603f * z; // Y10
    shBasis[3] = 0.488603f * x; // Y11
    shBasis[4] = 1.092548f * x * y; // Y2-2
    shBasis[5] = 1.092548f * y * z; // Y2-1
    shBasis[6] = 0.315392f * (3.0f * z * z - 1.0f); // Y20
    shBasis[7] = 1.092548f * x * z; // Y21
    shBasis[8] = 0.546274f * (x * x - y * y); // Y22

    // Combine SH basis functions with SH coefficients
    float3 result = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 9; ++i)
    {
        result += shCoefficients[i].xyz * shBasis[i];
    }

    return result;
}
