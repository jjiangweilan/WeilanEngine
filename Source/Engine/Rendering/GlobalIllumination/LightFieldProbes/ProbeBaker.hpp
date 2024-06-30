#include "GfxDriver/GfxDriver.hpp"
#include "Probe.hpp"
#include "Rendering/DrawList.hpp"
namespace Rendering::LFP
{
struct ProbeFace
{
    std::unique_ptr<Gfx::RenderPass> gbufferPass;
    std::unique_ptr<Gfx::ImageView> albedoView;
    std::unique_ptr<Gfx::ImageView> normalView;
    std::unique_ptr<Gfx::ImageView> depthView;
    std::unique_ptr<Gfx::Buffer> lfpBuffer;
    std::unique_ptr<Gfx::ShaderResource> set1Resource;

    void Init(
        Gfx::Image* albedoCubemap,
        Gfx::Image* normalCubemap,
        Gfx::Image* depthCubeMap,
        uint32_t face,
        glm::vec3 probePosition
    )
    {
        gbufferPass = GetGfxDriver()->CreateRenderPass();
        albedoView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *albedoCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}
        });

        normalView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *normalCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}
        });

        depthView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *depthCubeMap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Depth, 0, 1, face, 1}
        });

        Gfx::Attachment albedoAttachment{
            albedoView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::Attachment normalAttachment{
            normalView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::Attachment depthAttachment{
            depthView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };

        gbufferPass->AddSubpass({albedoAttachment, normalAttachment}, depthAttachment);
        lfpBuffer = GetGfxDriver()->CreateBuffer(
            Gfx::Buffer::CreateInfo{Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst, 64, false, "", false}
        );

        glm::vec3 cubeDir[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        };

        // cubemap face 0 projection matrix
        glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
        projection[1] = -projection[1];
        glm::mat4 view = glm::lookAtLH(
            probePosition,
            probePosition + cubeDir[face],
            (face != 2 && face != 3) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f)
        );
        glm::mat4 vp = projection * view;
        GetGfxDriver()->UploadBuffer(*lfpBuffer, (uint8_t*)&vp, sizeof(vp));
        set1Resource = GetGfxDriver()->CreateShaderResource();
        set1Resource->SetBuffer("LFP", lfpBuffer.get());
    };
};

/** TODO
  1. maybe implement a feature that let scene object write it's own "ShaderPass", instead of assuming StandardPBR
  */
class ProbeBaker
{
public:
    // the probe to bake to
    ProbeBaker(Probe& probe);

    void Bake(Gfx::CommandBuffer& cmd, DrawList* drawList);

    std::unique_ptr<Gfx::Image>& GetAlbedoCubemap()
    {
        return albedoCubemap;
    }

private:
    ProbeFace faces[6];
    std::unique_ptr<Gfx::Image> albedoCubemap;
    std::unique_ptr<Gfx::Image> normalCubemap;
    std::unique_ptr<Gfx::Image> depthCubeMap;
    Material reprojectMaterial;

    const uint32_t rtWidth = 128;
    const uint32_t rtHeight = 128;

    std::unique_ptr<Gfx::RenderPass> probeOctahedralPass;
    Probe* probe;
    Shader* probeCubemapShader;
    Shader* octahedralRemapShader;

    Shader* GetOctahedralRemapBaker();
};
} // namespace Rendering::LFP
