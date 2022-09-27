#include "GfxDriver/Vulkan/VKShaderInfo.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace Engine;

std::string json0Str = R"(
    {
        "Internal/ImGui.frag": {
            "entryPoints": [
                {
                    "name": "main",
                    "mode": "frag"
                }
            ],
            "types": {
                "_11": {
                    "name": "_11",
                    "members": [
                        {
                            "name": "Color",
                            "type": "vec4"
                        },
                        {
                            "name": "UV",
                            "type": "vec2"
                        }
                    ]
                }
            },
            "inputs": [
                {
                    "type": "_11",
                    "name": "In",
                    "location": 0
                }
            ],
            "outputs": [
                {
                    "type": "vec4",
                    "name": "fColor",
                    "location": 0
                }
            ],
            "textures": [
                {
                    "type": "sampler2D",
                    "name": "sTexture",
                    "set": 0,
                    "binding": 0
                }
            ],
            "spvPath": "Internal/ImGui.frag"
        }
    })";


TEST(Utils_ShaderModuleProcess, ProcessSceneLayout)
{
    std::string t0 = 
    R"(
        {
        "Internal/SceneLayout.frag": {
                "entryPoints": [
                    {
                        "name": "main",
                        "mode": "frag"
                    }
                ],
                "types": {
                    "_13": {
                        "name": "Transform",
                        "members": [
                            {
                                "name": "model",
                                "type": "mat4",
                                "offset": 0,
                                "matrix_stride": 16
                            }
                        ]
                    },
                    "_16": {
                        "name": "SceneInfo",
                        "members": [
                            {
                                "name": "view",
                                "type": "mat4",
                                "offset": 0,
                                "matrix_stride": 16
                            },
                            {
                                "name": "projection",
                                "type": "mat4",
                                "offset": 64,
                                "matrix_stride": 16
                            },
                            {
                                "name": "viewProjection",
                                "type": "mat4",
                                "offset": 128,
                                "matrix_stride": 16
                            }
                        ]
                    }
                },
                "outputs": [
                    {
                        "type": "vec4",
                        "name": "out_Color",
                        "location": 0
                    }
                ],
                "ubos": [
                    {
                        "type": "_16",
                        "name": "SceneInfo",
                        "block_size": 192,
                        "set": 0,
                        "binding": 0
                    }
                ],
                "push_constants": [
                    {
                        "type": "_13",
                        "name": "pushConstant",
                        "push_constant": true
                    }
                ],
                "spvPath": "Internal/SceneLayout.frag"
            }
            }
    )";
    nlohmann::json srJson = nlohmann::json::parse(t0)["Internal/SceneLayout.frag"];
    Gfx::ShaderInfo::ShaderStageInfo shaderStage;

    Gfx::ShaderInfo::Utils::Process(shaderStage, srJson);
}

TEST(Utils_ShaderModuleProcess, ProcessType)
{
    nlohmann::json srJson = nlohmann::json::parse(json0Str)["Internal/ImGui.frag"];
    Gfx::ShaderInfo::StructuredData data;
    Gfx::ShaderInfo::Utils::Process(data, srJson["inputs"][0]["type"], srJson, "");
    Gfx::ShaderInfo::StructuredData expected;
    expected.name = "_11";
    expected.type = Gfx::ShaderInfo::ShaderDataType::Structure;
    expected.size = 24;

    Gfx::ShaderInfo::Member member;
    member.name = "UV";
    member.dimension = {1};
    member.offset = 0;
    member.data = MakeUnique<Gfx::ShaderInfo::StructuredData>();
    member.data->name = "vec4";
    member.data->size = 16;
    member.data->type = Gfx::ShaderInfo::ShaderDataType::Vec4;
    expected.members["vec4"] = member;
    member.name = "Color";
    member.dimension = {1};
    member.offset = 16;
    member.data = MakeUnique<Gfx::ShaderInfo::StructuredData>();
    member.data->name = "vec2";
    member.data->size = 8;
    member.data->type = Gfx::ShaderInfo::ShaderDataType::Vec2;
    expected.members["vec2"] = member;
    EXPECT_EQ(expected, data);
}
