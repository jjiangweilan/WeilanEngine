#include "EnvironmentBaker.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Utils/AssetLoader.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
namespace Engine::Editor
{
EnvironmentBaker::EnvironmentBaker() {}

void EnvironmentBaker::CreateRenderData(uint32_t width, uint32_t height)
{
    GetGfxDriver()->WaitForIdle();
    sceneImage = GetGfxDriver()->CreateImage(
        {
            .width = width,
            .height = height,
            .format = Gfx::ImageFormat::R8G8B8A8_SRGB,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
            .isCubemap = false,
        },
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );
}

static std::unique_ptr<Gfx::ShaderProgram> LoadShader(const char* path)
{
    auto shaderCompiler = ShaderCompiler();
    std::fstream f;
    f.open(path);
    if (f.is_open() && f.good())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        shaderCompiler.Compile(ss.str(), true);
        return GetGfxDriver()->CreateShaderProgram(
            "EnvironmentBaker",
            &shaderCompiler.GetConfig(),
            shaderCompiler.GetVertexSPV(),
            shaderCompiler.GetFragSPV()
        );
    }
    else
    {
        SPDLOG_ERROR("EnvironmentBaker: failed to open EnvironmentBaker.shad");
    }

    return nullptr;
}

void EnvironmentBaker::Bake(int size)
{
    uint32_t minimumSize = glm::pow(2, 5);
    if (size < minimumSize)
        return;

    GetGfxDriver()->WaitForIdle();

    // create baking shader if it's not created
    if (lightingBaker == nullptr)
    {
        try
        {
            lightingBaker = LoadShader("Assets/Shaders/Editor/EnvrionmentBaker/EnvironmentLightBaker.shad");
        }
        catch (...)
        {
            lightingBaker = nullptr;
            return;
        }
    }

    if (cubemap == nullptr)
    {
        cubemap = GetGfxDriver()->CreateImage(
            {
                .width = (uint32_t)size,
                .height = (uint32_t)size,
                .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
                .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                .mipLevels = 1,
                .isCubemap = true,
            },
            Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture
        );
    }

    if (brdfBaker == nullptr)
    {
        // brdfBaker = LoadShader("Assets/Shaders/Editor/EnvrionmentBaker/EnvironmentBRDFBaker.shad");
    }

    // create baking resource if it's not created
    if (bakingShaderResource == nullptr)
    {
        bakingShaderResource =
            GetGfxDriver()->CreateShaderResource(lightingBaker.get(), Gfx::ShaderResourceFrequency::Global);
        bakeInfoBuffer = GetGfxDriver()->CreateBuffer({
            .usages = Gfx::BufferUsage::Uniform,
            .size = sizeof(ShaderParamBakeInfo),
            .visibleInCPU = true,
        });
        bakingShaderResource->SetBuffer(*bakeInfoBuffer, 0);
    }

    if (environmentMap == nullptr)
    {
        auto envMapBinary = AssetsLoader::Instance()->ReadAsBinary("Assets/envMap.ktx");
        KtxTexture data{
            .imageData = envMapBinary.data(),
            .byteSize = envMapBinary.size(),
        };
        environmentMap = std::make_unique<Texture>(data);
        bakingShaderResource->SetImage("environmentMap", environmentMap->GetGfxImage());
    }

    // placing each face in front of the camera view in ndc space
    ShaderParamBakeInfo bakeInfos[6] = {
        {
            // x+
            .uFrom = {1, 1, 1, 1},
            .uTo = {1, 1, -1, 1},
            .vFrom = {1, -1, 1, 1},
            .vTo = {1, -1, -1, 1},
            .roughness = roughness,
        },
        {
            // x-
            .uFrom = {-1, 1, -1, 1},
            .uTo = {-1, 1, 1, 1},
            .vFrom = {-1, -1, -1, 1},
            .vTo = {-1, -1, 1, 1},
            .roughness = roughness,
        },
        {
            // y+
            .uFrom = {-1, 1, -1, 1},
            .uTo = {1, 1, -1, 1},
            .vFrom = {-1, 1, 1, 1},
            .vTo = {1, 1, 1, 1},
            .roughness = roughness,
        },
        {
            // y-
            .uFrom = {-1, -1, 1, 1},
            .uTo = {1, -1, 1, 1},
            .vFrom = {-1, -1, -1, 1},
            .vTo = {1, -1, -1, 1},
            .roughness = roughness,
        },
        {
            // z+
            .uFrom = {-1, 1, 1, 1},
            .uTo = {1, 1, 1, 1},
            .vFrom = {-1, -1, 1, 1},
            .vTo = {1, -1, 1, 1},
            .roughness = roughness,
        },
        {
            // z-
            .uFrom = {1, 1, -1, 1},
            .uTo = {-1, 1, -1, 1},
            .vFrom = {1, -1, -1, 1},
            .vTo = {-1, -1, -1, 1},
            .roughness = roughness,
        },
    };
    for (int face = 0; face < 6; ++face)
    {
        BakeToCubeFace(*cubemap, face, bakeInfos[face]);
    }

    auto buffer = ImmediateGfx::CopyImageToBuffer(*cubemap);
    auto& desc = cubemap->GetDescription();
    Exporters::KtxExporter::Export(
        "envCubemap.ktx",
        (uint8_t*)buffer->GetCPUVisibleAddress(),
        buffer->GetSize(),
        desc.width,
        desc.height,
        1,
        2,
        6,
        1,
        false,
        true,
        desc.format
    );
}

void EnvironmentBaker::BakeToCubeFace(Gfx::Image& cubemap, uint32_t layer, ShaderParamBakeInfo bakeInfo)
{
    ShaderParamBakeInfo* bakeInfoBuf = (ShaderParamBakeInfo*)bakeInfoBuffer->GetCPUVisibleAddress();
    *bakeInfoBuf = bakeInfo;
    ImmediateGfx::RenderToImage(
        cubemap,
        {
            .aspectMask = Gfx::ImageAspectFlags::Color,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        },
        *lightingBaker,
        lightingBaker->GetDefaultShaderConfig(),
        *bakingShaderResource,
        Gfx::ImageLayout::Transfer_Src
    );
}

bool EnvironmentBaker::Tick()
{
    bool open = true;

    ImGui::Begin("Environment Baker", &open);
    // create scene color if it's null or if the window size is changed
    auto contentMax = ImGui::GetWindowContentRegionMax();
    auto contentMin = ImGui::GetWindowContentRegionMin();
    float width = contentMax.x - contentMin.x;
    float height = contentMax.y - contentMin.y;
    if (sceneImage == nullptr || sceneImage->GetDescription().width != (uint32_t)width ||
        sceneImage->GetDescription().height != (uint32_t)height)
    {
        // CreateRenderData(width, height);
    }

    ImGui::InputInt("resolution", &size);
    ImGui::InputFloat("roughness", &roughness);
    if (ImGui::Button("Bake"))
    {
        Bake(size);
    }

    if (cubemap != nullptr)
    {
        if (viewResults.empty())
        {
            for (int face = 0; face < 6; face++)
            {
                std::unique_ptr<Gfx::ImageView> imageView = GetGfxDriver()->CreateImageView(
                    {*cubemap, Gfx::ImageViewType::Image_2D, {Gfx::ImageAspect::Color, 0, 1, (uint32_t)face, 1}}
                );
                viewResults.push_back(std::move(imageView));
            }
        }

        for (int face = 0; face < 6; face++)
        {
            // ImGui::Image(
            //     viewResults[face].get(),
            //     ImVec2{
            //         (float)cubemap->GetDescription().width,
            //         (float)cubemap->GetDescription().height,
            //     }
            // );
        }
    }

    ImGui::End();
    return open;
}

void CreateCubemapPreviewGraph(Gfx::Image* target)
{
    RenderGraph::Graph graph;
    graph.AddNode(
        [](Gfx::CommandBuffer& cmd, auto& renderpass, const auto& res) {},
        {
            {
                .name = "target",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .externalImage = target,
            },
            {
                .name = "targe depth",
                .handle = 1,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags =
                    Gfx::AccessMask::Depth_Stencil_Attachment_Write | Gfx::AccessMask::Depth_Stencil_Attachment_Read,
                .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests,
                .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
                .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                .imageCreateInfo =
                    {
                        .width = target->GetDescription().width,
                        .height = target->GetDescription().height,
                        .format = Gfx::ImageFormat::D24_UNorm_S8_UInt,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
        },
        {
            {
                .colors =
                    {
                        {
                            .handle = 0,
                        },
                    },
                .depth = {{.handle = 1}},
            },
        }
    );
}

void EnvironmentBaker::Render(Gfx::CommandBuffer& cmd) {}

void EnvironmentBaker::Open() {}
void EnvironmentBaker::Close()
{
    sceneImage = nullptr;
}
} // namespace Engine::Editor
