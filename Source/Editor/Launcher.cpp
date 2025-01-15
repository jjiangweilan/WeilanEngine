#include "GameEditor.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include "Libs/Math.hpp"
using namespace glm;
class Launcher
{
    using ArgList = std::vector<std::string_view>;

public:
    Launcher() {}
    float GetHeightFractionForPoint(float3 inPosition, float2 inCloudMinMax)
    {
        // get global fractional position in cloud zone
        float height_fraction = (inPosition.z - inCloudMinMax.x) / (inCloudMinMax.y - inCloudMinMax.x);

        return saturate(height_fraction);
    }

    // Utility function that maps a value from one range to another.
    float Remap(float original_value, float original_min, float original_max, float new_min, float new_max)
    {
        return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
    }


    float SampleCloudDensity(float3 p, float3 weather_data, int mip_level, bool doCheaply)
    {

        // get height fraction  (be sure to create a cloud_min_max variable)
        float height_fraction = GetHeightFractionForPoint(p, { 0.2,0.8 });

        // wind settings
        float3 wind_direction = float3(1.0, 0.0, 0.0);
        float cloud_speed = 10.0;

        // cloud_top offset - push the tops of the clouds along this wind direction by this many units.
        float cloud_top_offset = 500.0;

        // skew in wind direction
        p += height_fraction * wind_direction * cloud_top_offset;

        //animate clouds in wind direction and add a small upward bias to the wind direction
        p += (wind_direction + float3(0.0, 0.1, 0.0)) * 1.0f * cloud_speed;

        // read the low frequency Perlin-Worley and Worley noises
        float4 low_frequency_noises = float4(0.4, 0.6, 0.3, 0.1);

        // build an fBm out of  the low frequency Worley noises that can be used to add detail to the Low frequency Perlin-Worley noise
        float low_freq_fBm = (low_frequency_noises.g * 0.625) + (low_frequency_noises.b * 0.25) + (low_frequency_noises.a * 0.125);

        // define the base cloud shape by dilating it with the low frequency fBm made of Worley noise.
        float base_cloud = Remap(low_frequency_noises.r, -(1.0 - low_freq_fBm), 1.0, 0.0, 1.0);

        // Get the density-height gradient using the density-height function (not included)
        float density_height_gradient = 0.7;

        // apply the height function to the base cloud shape
        base_cloud *= density_height_gradient;

        // cloud coverage is stored in the weather_data¡¯s red channel.
        float cloud_coverage = weather_data.r;

        // apply anvil deformations
        cloud_coverage = pow(cloud_coverage, Remap(height_fraction, 0.7, 0.8, 1.0, glm::mix(1.0, 0.5, 0.3)));

        //Use remapper to apply cloud coverage attribute
        float base_cloud_with_coverage = Remap(base_cloud, 8, 1.0, 0.0, 1.0);

        //Multiply result by cloud coverage so that smaller clouds are lighter and more aesthetically pleasing.
        base_cloud_with_coverage *= cloud_coverage;

        //define final cloud value
        float final_cloud = base_cloud_with_coverage;

        return final_cloud;
    }

    void ArgsParser(int argc, char** argv)
    {
        argList = std::make_unique<ArgList>(argv + 1, argv + argc);
        for (int i = 0; i < argList->size(); ++i)
        {
            DispatchArgs(*argList, i);
        }

        if (!hasAction)
        {
            SPDLOG_ERROR("No action taken, maybe you should set a project path using --project");
        }
    }

    void DispatchArgs(ArgList& args, int& curr)
    {
        SampleCloudDensity({ 0.3,0.4,0.1 }, { 0.3,0.5,0.1 }, 0, false);
        if (args[curr] == "--project" || args[curr] == "-p")
        {
            curr++;
            std::filesystem::path path(args[curr]);
            // if (std::filesystem::exists(path))
            // {}
            // else
            // {
            //     SPDLOG_ERROR("{} is not a valid path", path.string());
            // }

            auto editor = std::make_unique<Editor::GameEditor>(path.string().c_str());
            editor->Start();

            hasAction = true;
        }
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

#undef main

int main(int argc, char** argv)
{
    Launcher l;
    l.ArgsParser(argc, argv);
}
