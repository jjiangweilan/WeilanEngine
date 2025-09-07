#include "bend_sss_cpu.hpp"

namespace Bend
{
DispatchList BuildDispatchList(
    float inLightProjection[4],
    int inViewportSize[2],
    int inMinRenderBounds[2],
    int inMaxRenderBounds[2],
    bool inExpandedZRange,
    int inWaveSize
)
{
    DispatchList result{};

    // Floating point division in the shader has a practical limit for precision when the light is *very* far off screen
    // (~1m pixels+) So when computing the light XY coordinate, use an adjusted w value to handle these extreme values
    float xy_light_w = inLightProjection[3];
    float FP_limit = 0.000002f * (float)inWaveSize;

    if (xy_light_w >= 0 && xy_light_w < FP_limit)
        xy_light_w = FP_limit;
    else if (xy_light_w < 0 && xy_light_w > -FP_limit)
        xy_light_w = -FP_limit;

    // Need precise XY pixel coordinates of the light
    result.LightCoordinate_Shader[0] = ((inLightProjection[0] / xy_light_w) * +0.5f + 0.5f) * (float)inViewportSize[0];
    result.LightCoordinate_Shader[1] = ((inLightProjection[1] / xy_light_w) * -0.5f + 0.5f) * (float)inViewportSize[1];
    result.LightCoordinate_Shader[2] = inLightProjection[3] == 0 ? 0 : (inLightProjection[2] / inLightProjection[3]);
    result.LightCoordinate_Shader[3] = inLightProjection[3] > 0 ? 1 : -1;

    if (inExpandedZRange)
    {
        result.LightCoordinate_Shader[2] = result.LightCoordinate_Shader[2] * 0.5f + 0.5f;
    }

    int light_xy[2] = {(int)(result.LightCoordinate_Shader[0] + 0.5f), (int)(result.LightCoordinate_Shader[1] + 0.5f)};

    // Make the bounds inclusive, relative to the light
    const int biased_bounds[4] = {
        inMinRenderBounds[0] - light_xy[0],
        -(inMaxRenderBounds[1] - light_xy[1]),
        inMaxRenderBounds[0] - light_xy[0],
        -(inMinRenderBounds[1] - light_xy[1]),
    };

    // Process 4 quadrants around the light center,
    // They each form a rectangle with one corner on the light XY coordinate
    // If the rectangle isn't square, it will need breaking in two on the larger axis
    // 0 = bottom left, 1 = bottom right, 2 = top left, 2 = top right
    for (int q = 0; q < 4; q++)
    {
        // Quads 0 and 3 needs to be +1 vertically, 1 and 2 need to be +1 horizontally
        bool vertical = q == 0 || q == 3;

        // Bounds relative to the quadrant
        const int bounds[4] = {
            bend_max(0, ((q & 1) ? biased_bounds[0] : -biased_bounds[2])) / inWaveSize,
            bend_max(0, ((q & 2) ? biased_bounds[1] : -biased_bounds[3])) / inWaveSize,
            bend_max(0, (((q & 1) ? biased_bounds[2] : -biased_bounds[0]) + inWaveSize * (vertical ? 1 : 2) - 1)) /
                inWaveSize,
            bend_max(0, (((q & 2) ? biased_bounds[3] : -biased_bounds[1]) + inWaveSize * (vertical ? 2 : 1) - 1)) /
                inWaveSize,
        };

        if ((bounds[2] - bounds[0]) > 0 && (bounds[3] - bounds[1]) > 0)
        {
            int bias_x = (q == 2 || q == 3) ? 1 : 0;
            int bias_y = (q == 1 || q == 3) ? 1 : 0;

            DispatchData& disp = result.Dispatch[result.DispatchCount++];

            disp.WaveCount[0] = inWaveSize;
            disp.WaveCount[1] = bounds[2] - bounds[0];
            disp.WaveCount[2] = bounds[3] - bounds[1];
            disp.WaveOffset_Shader[0] = ((q & 1) ? bounds[0] : -bounds[2]) + bias_x;
            disp.WaveOffset_Shader[1] = ((q & 2) ? -bounds[3] : bounds[1]) + bias_y;

            // We want the far corner of this quadrant relative to the light,
            // as we need to know where the diagonal light ray intersects with the edge of the bounds
            int axis_delta = +biased_bounds[0] - biased_bounds[1];
            if (q == 1)
                axis_delta = +biased_bounds[2] + biased_bounds[1];
            if (q == 2)
                axis_delta = -biased_bounds[0] - biased_bounds[3];
            if (q == 3)
                axis_delta = -biased_bounds[2] + biased_bounds[3];

            axis_delta = (axis_delta + inWaveSize - 1) / inWaveSize;

            if (axis_delta > 0)
            {
                DispatchData& disp2 = result.Dispatch[result.DispatchCount++];

                // Take copy of current volume
                disp2 = disp;

                if (q == 0)
                {
                    // Split on Y, split becomes -1 larger on x
                    disp2.WaveCount[2] = bend_min(disp.WaveCount[2], axis_delta);
                    disp.WaveCount[2] -= disp2.WaveCount[2];
                    disp2.WaveOffset_Shader[1] = disp.WaveOffset_Shader[1] + disp.WaveCount[2];
                    disp2.WaveOffset_Shader[0]--;
                    disp2.WaveCount[1]++;
                }
                if (q == 1)
                {
                    // Split on X, split becomes +1 larger on y
                    disp2.WaveCount[1] = bend_min(disp.WaveCount[1], axis_delta);
                    disp.WaveCount[1] -= disp2.WaveCount[1];
                    disp2.WaveOffset_Shader[0] = disp.WaveOffset_Shader[0] + disp.WaveCount[1];
                    disp2.WaveCount[2]++;
                }
                if (q == 2)
                {
                    // Split on X, split becomes -1 larger on y
                    disp2.WaveCount[1] = bend_min(disp.WaveCount[1], axis_delta);
                    disp.WaveCount[1] -= disp2.WaveCount[1];
                    disp.WaveOffset_Shader[0] += disp2.WaveCount[1];
                    disp2.WaveCount[2]++;
                    disp2.WaveOffset_Shader[1]--;
                }
                if (q == 3)
                {
                    // Split on Y, split becomes +1 larger on x
                    disp2.WaveCount[2] = bend_min(disp.WaveCount[2], axis_delta);
                    disp.WaveCount[2] -= disp2.WaveCount[2];
                    disp.WaveOffset_Shader[1] += disp2.WaveCount[2];
                    disp2.WaveCount[1]++;
                }

                // Remove if too small
                if (disp2.WaveCount[1] <= 0 || disp2.WaveCount[2] <= 0)
                {
                    disp2 = result.Dispatch[--result.DispatchCount];
                }
                if (disp.WaveCount[1] <= 0 || disp.WaveCount[2] <= 0)
                {
                    disp = result.Dispatch[--result.DispatchCount];
                }
            }
        }
    }

    // Scale the shader values by the wave count, the shader expects this
    for (int i = 0; i < result.DispatchCount; i++)
    {
        result.Dispatch[i].WaveOffset_Shader[0] *= inWaveSize;
        result.Dispatch[i].WaveOffset_Shader[1] *= inWaveSize;
    }

    return result;
}
} // namespace Bend
