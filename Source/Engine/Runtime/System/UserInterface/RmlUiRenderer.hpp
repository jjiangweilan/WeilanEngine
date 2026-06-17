#pragma once

#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>

class RmlUiRenderer : public Rml::RenderInterface
{
public:
    RmlUiRenderer();
    ~RmlUiRenderer() override;

    void BeginFrame(Gfx::CommandBuffer& cmd, int2 viewportOrigin, int2 viewportSize);
    void EndFrame();

    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices
    ) override;
    void RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture
    ) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i sourceDimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* transform) override;

private:
    struct Geometry
    {
        std::unique_ptr<Gfx::Buffer> vertexBuffer;
        std::unique_ptr<Gfx::Buffer> indexBuffer;
        uint32_t indexCount = 0;
    };

    struct TextureData
    {
        std::unique_ptr<Texture> texture;
        std::unique_ptr<Gfx::ShaderResource> shaderResource;
    };

    struct PushConstant
    {
        glm::mat4 transform = glm::mat4(1.0f);
        glm::vec2 scale = {};
        glm::vec2 translate = {};
        glm::vec2 geometryTranslate = {};
        float useTexture = 0.0f;
        float padding[3] = {};
    };

    Gfx::CommandBuffer* activeCmd = nullptr;
    int2 activeViewportOrigin = {0, 0};
    int2 activeViewportSize = {1, 1};
    bool scissorEnabled = false;
    Rml::Rectanglei scissorRegion = Rml::Rectanglei::MakeInvalid();
    glm::mat4 activeTransform = glm::mat4(1.0f);

    ObjPtr<Shader> shader = nullptr;
    ObjPtr<Texture> whiteTexture = nullptr;
    std::unordered_map<UUID, std::unique_ptr<Gfx::ShaderResource>> textureResourceCache;

    void ApplyScissor();
    void BindTexture(Texture* texture);
    VertexAttributes GetVertexAttributes() const;
};
