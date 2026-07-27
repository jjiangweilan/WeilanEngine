#include "RmlUiRenderer.hpp"

#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Core/DelayDestroy.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <spdlog/spdlog.h>

namespace
{
constexpr size_t InitialVertexBufferSize = 64 * 1024;
constexpr size_t InitialIndexBufferSize = 32 * 1024;

struct TextureSource
{
    std::string path;
    bool usePointFilter = false;
};

TextureSource ParseTextureSource(const Rml::String& source)
{
    TextureSource result;

    const size_t queryStart = source.find('?');
    if (queryStart == Rml::String::npos)
    {
        result.path = source;
        return result;
    }

    result.path = source.substr(0, queryStart);
    const Rml::String query = source.substr(queryStart + 1);
    result.usePointFilter = query.find("filter=point") != Rml::String::npos || query.find("filter=nearest") != Rml::String::npos;
    return result;
}

size_t GrowCapacity(size_t currentCapacity, size_t requiredCapacity, size_t minimumCapacity)
{
    size_t capacity = std::max(currentCapacity, minimumCapacity);
    requiredCapacity = std::max(requiredCapacity, minimumCapacity);
    while (capacity < requiredCapacity && capacity <= std::numeric_limits<size_t>::max() / 2)
    {
        capacity *= 2;
    }
    return std::max(capacity, requiredCapacity);
}
} // namespace

RmlUiRenderer::RmlUiRenderer()
{
    shader = ShaderLibrary::GetShader(Shaders::RmlUi);
    whiteTexture = &EngineInternalResources::GetWhiteTexture();
    CreateGeometryBlock(InitialVertexBufferSize, InitialIndexBufferSize);
}

RmlUiRenderer::~RmlUiRenderer()
{
    for (auto& block : geometryBlocks)
    {
        if (block->vertexBuffer)
        {
            DelayDestroy::Singleton()->Destory(std::move(block->vertexBuffer));
        }
        if (block->indexBuffer)
        {
            DelayDestroy::Singleton()->Destory(std::move(block->indexBuffer));
        }
    }
    geometryBlocks.clear();
}

void RmlUiRenderer::BeginFrame(Gfx::CommandBuffer& cmd, int2 viewportOrigin, int2 viewportSize)
{
    activeCmd = &cmd;
    activeViewportOrigin = viewportOrigin;
    activeViewportSize = {std::max(1, viewportSize.x), std::max(1, viewportSize.y)};

    Gfx::Viewport viewport;
    viewport.x = static_cast<float>(activeViewportOrigin.x);
    viewport.y = static_cast<float>(activeViewportOrigin.y);
    viewport.width = static_cast<float>(activeViewportSize.x);
    viewport.height = static_cast<float>(activeViewportSize.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    activeCmd->SetViewport(viewport);

    scissorEnabled = false;
    ApplyScissor();
}

void RmlUiRenderer::EndFrame()
{
    activeCmd = nullptr;
}

Rml::CompiledGeometryHandle RmlUiRenderer::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices
)
{
    if (vertices.empty() || indices.empty())
    {
        return {};
    }

    auto geometry = std::make_unique<Geometry>();
    const size_t vertexSize = vertices.size() * sizeof(Rml::Vertex);
    const size_t indexSize = indices.size() * sizeof(int);

    if (!AllocateGeometryRanges(*geometry, vertexSize, indexSize))
    {
        AddGeometryBlock(vertexSize, indexSize);
        if (!AllocateGeometryRanges(*geometry, vertexSize, indexSize))
        {
            spdlog::error("RmlUi failed to allocate geometry buffers: vertex bytes {}, index bytes {}", vertexSize, indexSize);
            return {};
        }
    }

    geometry->indexCount = static_cast<uint32_t>(indices.size());

    GetGfxDriver()->UploadBuffer(*geometry->block->vertexBuffer, (uint8_t*)vertices.data(), vertexSize, geometry->vertexOffset);
    GetGfxDriver()->UploadBuffer(*geometry->block->indexBuffer, (uint8_t*)indices.data(), indexSize, geometry->indexOffset);

    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry.release());
}

void RmlUiRenderer::RenderGeometry(
    Rml::CompiledGeometryHandle geometryHandle,
    Rml::Vector2f translation,
    Rml::TextureHandle textureHandle
)
{
    if (!activeCmd || !geometryHandle || !shader || activeViewportSize.x <= 0 || activeViewportSize.y <= 0)
    {
        return;
    }

    Geometry* geometry = reinterpret_cast<Geometry*>(geometryHandle);
    Gfx::ShaderProgram* shaderProgram = shader->GetShaderProgram();
    if (!shaderProgram || !geometry->block || !geometry->block->vertexBuffer || !geometry->block->indexBuffer)
    {
        return;
    }

    Texture* texture = whiteTexture.Get();
    float useTexture = 0.0f;
    float usePointFilter = 0.0f;
    if (textureHandle)
    {
        TextureData* textureData = reinterpret_cast<TextureData*>(textureHandle);
        texture = textureData->texture.get();
        useTexture = 1.0f;
        usePointFilter = textureData->usePointFilter ? 1.0f : 0.0f;
    }

    BindTexture(texture);
    ApplyScissor();

    PushConstant pushConstant;
    pushConstant.transform = activeTransform;
    pushConstant.scale = {2.0f / static_cast<float>(activeViewportSize.x), 2.0f / static_cast<float>(activeViewportSize.y)};
    pushConstant.translate = {-1.0f, -1.0f};
    pushConstant.geometryTranslate = {translation.x, translation.y};
    pushConstant.useTexture = useTexture;
    pushConstant.usePointFilter = usePointFilter;

    Gfx::VertexBufferBinding vertexBinding[] = {{geometry->block->vertexBuffer.get(), geometry->vertexOffset}};
    activeCmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());
    activeCmd->BindVertexBuffer(vertexBinding, 0);
    activeCmd->BindIndexBuffer(geometry->block->indexBuffer.get(), geometry->indexOffset, Gfx::IndexBufferType::UInt32);
    activeCmd->SetPushConstant(shaderProgram, &pushConstant);
    activeCmd->DrawIndexed(geometry->indexCount, 1, 0, 0, 0);
}

void RmlUiRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    Geometry* geometryData = reinterpret_cast<Geometry*>(geometry);
    if (!geometryData)
    {
        return;
    }

    if (geometryData->block && geometryData->block->vertexAllocator)
    {
        geometryData->block->vertexAllocator->Free(geometryData->vertexAllocation);
    }
    if (geometryData->block && geometryData->block->indexAllocator)
    {
        geometryData->block->indexAllocator->Free(geometryData->indexAllocation);
    }
    delete geometryData;
}

Rml::TextureHandle RmlUiRenderer::LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source)
{
    try
    {
        const TextureSource textureSource = ParseTextureSource(source);
        auto textureData = std::make_unique<TextureData>();
        textureData->usePointFilter = textureSource.usePointFilter;
        textureData->texture = std::make_unique<Texture>(textureSource.path.c_str());
        textureData->texture->SetName(source);
        textureDimensions = {
            static_cast<int>(textureData->texture->GetDescription().img.width),
            static_cast<int>(textureData->texture->GetDescription().img.height)
        };
        textureData->shaderResource = GetGfxDriver()->CreateShaderResource();
        textureData->shaderResource->SetImage("texture", &textureData->texture->GetGfxImage()->GetDefaultImageView());
        return reinterpret_cast<Rml::TextureHandle>(textureData.release());
    }
    catch (const std::exception& e)
    {
        spdlog::warn("RmlUi failed to load texture '{}': {}", source, e.what());
        return {};
    }
}

Rml::TextureHandle RmlUiRenderer::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i sourceDimensions)
{
    if (source.empty() || sourceDimensions.x <= 0 || sourceDimensions.y <= 0)
    {
        return {};
    }

    const size_t byteSize = source.size();
    uint8_t* data = new uint8_t[byteSize];
    memcpy(data, source.data(), byteSize);

    TextureDescription desc;
    desc.img.width = static_cast<uint32_t>(sourceDimensions.x);
    desc.img.height = static_cast<uint32_t>(sourceDimensions.y);
    desc.img.depth = 1;
    desc.img.mipLevels = 1;
    desc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    desc.img.isCubemap = false;
    desc.img.format = Gfx::GfxFormat::R8G8B8A8_UNorm;
    desc.data = data;
    desc.keepData = false;

    auto textureData = std::make_unique<TextureData>();
    textureData->texture = std::make_unique<Texture>(desc);
    textureData->texture->SetName("RmlUi Generated Texture");
    textureData->shaderResource = GetGfxDriver()->CreateShaderResource();
    textureData->shaderResource->SetImage("texture", &textureData->texture->GetGfxImage()->GetDefaultImageView());

    return reinterpret_cast<Rml::TextureHandle>(textureData.release());
}

void RmlUiRenderer::ReleaseTexture(Rml::TextureHandle texture)
{
    delete reinterpret_cast<TextureData*>(texture);
}

void RmlUiRenderer::EnableScissorRegion(bool enable)
{
    scissorEnabled = enable;
}

void RmlUiRenderer::SetScissorRegion(Rml::Rectanglei region)
{
    scissorRegion = region;
}

void RmlUiRenderer::SetTransform(const Rml::Matrix4f* transform)
{
    activeTransform = transform ? glm::make_mat4(transform->data()) : glm::mat4(1.0f);
}

void RmlUiRenderer::ApplyScissor()
{
    if (!activeCmd)
    {
        return;
    }

    Rect2D scissor;
    if (scissorEnabled && scissorRegion.Valid())
    {
        const int minX = activeViewportOrigin.x;
        const int minY = activeViewportOrigin.y;
        const int maxX = activeViewportOrigin.x + activeViewportSize.x;
        const int maxY = activeViewportOrigin.y + activeViewportSize.y;

        const int left = std::clamp(activeViewportOrigin.x + scissorRegion.Left(), minX, maxX);
        const int top = std::clamp(activeViewportOrigin.y + scissorRegion.Top(), minY, maxY);
        const int right = std::clamp(activeViewportOrigin.x + scissorRegion.Right(), minX, maxX);
        const int bottom = std::clamp(activeViewportOrigin.y + scissorRegion.Bottom(), minY, maxY);

        scissor.offset.x = left;
        scissor.offset.y = top;
        scissor.extent.width = static_cast<uint32_t>(std::max(0, right - left));
        scissor.extent.height = static_cast<uint32_t>(std::max(0, bottom - top));
    }
    else
    {
        scissor.offset.x = activeViewportOrigin.x;
        scissor.offset.y = activeViewportOrigin.y;
        scissor.extent.width = static_cast<uint32_t>(activeViewportSize.x);
        scissor.extent.height = static_cast<uint32_t>(activeViewportSize.y);
    }

    activeCmd->SetScissor(0, 1, &scissor);
}

void RmlUiRenderer::BindTexture(Texture* texture)
{
    if (!activeCmd || !texture || !texture->GetGfxImage())
    {
        return;
    }

    const UUID& uuid = texture->GetGfxImage()->GetDefaultImageView().GetUUID();
    auto iter = textureResourceCache.find(uuid);
    if (iter != textureResourceCache.end())
    {
        activeCmd->BindResource(shader->GetSet(Gfx::DescriptorSetSemantics::Material), iter->second.get());
        return;
    }

    auto shaderResource = GetGfxDriver()->CreateShaderResource();
    shaderResource->SetImage("texture", &texture->GetGfxImage()->GetDefaultImageView());
    Gfx::ShaderResource* resource = shaderResource.get();
    textureResourceCache.emplace(uuid, std::move(shaderResource));
    activeCmd->BindResource(shader->GetSet(Gfx::DescriptorSetSemantics::Material), resource);
}

VertexAttributes RmlUiRenderer::GetVertexAttributes() const
{
    VertexAttributes attributes;
    attributes.AddAttribute("position", VertexAttributeSemantics::Position, 0, 8);
    attributes.AddAttribute("color", VertexAttributeSemantics::Color, 0, 4);
    attributes.AddAttribute("uv", VertexAttributeSemantics::Texcoord, 0, 8);
    return attributes;
}

RmlUiRenderer::GeometryBlock* RmlUiRenderer::CreateGeometryBlock(size_t vertexCapacity, size_t indexCapacity)
{
    vertexCapacity = std::max<size_t>(vertexCapacity, 1);
    indexCapacity = std::max<size_t>(indexCapacity, 1);

    auto block = std::make_unique<GeometryBlock>();
    block->vertexCapacity = vertexCapacity;
    block->indexCapacity = indexCapacity;
    block->vertexBuffer = GetGfxDriver()->CreateBuffer(
        {Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst, vertexCapacity, false, "RmlUi Persistent Vertex Buffer"}
    );
    block->vertexBuffer->SetVertexAttributes(0, GetVertexAttributes());

    block->indexBuffer = GetGfxDriver()->CreateBuffer(
        {Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst, indexCapacity, false, "RmlUi Persistent Index Buffer"}
    );

    block->vertexAllocator = std::make_unique<VirtualTLSFAllocator>(vertexCapacity);
    block->indexAllocator = std::make_unique<VirtualTLSFAllocator>(indexCapacity);

    GeometryBlock* result = block.get();
    geometryBlocks.push_back(std::move(block));
    return result;
}

bool RmlUiRenderer::AllocateGeometryRanges(Geometry& geometry, size_t vertexSize, size_t indexSize)
{
    for (auto& block : geometryBlocks)
    {
        if (!block->vertexAllocator || !block->indexAllocator)
        {
            continue;
        }

        VirtualTLSFAllocator::Allocation vertexAllocation;
        VirtualTLSFAllocator::Allocation indexAllocation;
        if (!block->vertexAllocator->Allocate(vertexSize, static_cast<uint32_t>(alignof(Rml::Vertex)), vertexAllocation))
        {
            continue;
        }
        if (!block->indexAllocator->Allocate(indexSize, static_cast<uint32_t>(alignof(int)), indexAllocation))
        {
            block->vertexAllocator->Free(vertexAllocation);
            continue;
        }

        geometry.block = block.get();
        geometry.vertexAllocation = vertexAllocation;
        geometry.indexAllocation = indexAllocation;
        geometry.vertexOffset = static_cast<size_t>(vertexAllocation.offset);
        geometry.indexOffset = static_cast<size_t>(indexAllocation.offset);
        geometry.vertexSize = vertexSize;
        geometry.indexSize = indexSize;
        return true;
    }

    return false;
}

RmlUiRenderer::GeometryBlock* RmlUiRenderer::AddGeometryBlock(size_t requiredVertexSize, size_t requiredIndexSize)
{
    size_t largestVertexCapacity = InitialVertexBufferSize;
    size_t largestIndexCapacity = InitialIndexBufferSize;
    for (const auto& block : geometryBlocks)
    {
        largestVertexCapacity = std::max(largestVertexCapacity, block->vertexCapacity);
        largestIndexCapacity = std::max(largestIndexCapacity, block->indexCapacity);
    }

    const size_t vertexCapacity = GrowCapacity(largestVertexCapacity, requiredVertexSize, InitialVertexBufferSize);
    const size_t indexCapacity = GrowCapacity(largestIndexCapacity, requiredIndexSize, InitialIndexBufferSize);
    return CreateGeometryBlock(vertexCapacity, indexCapacity);
}
