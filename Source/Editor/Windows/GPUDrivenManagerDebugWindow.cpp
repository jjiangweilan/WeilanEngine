#include "GPUDrivenManagerDebugWindow.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fmt/format.h>

using namespace Rendering;

namespace Editor
{

DEFINE_EDITOR_WINDOW(GPUDrivenManagerDebugWindow, "Debug/GPU Driven Manager")

namespace
{

ImVec4 UsageColor(float ratio)
{
    if (ratio < 0.6f)
        return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    if (ratio < 0.85f)
        return ImVec4(1.0f, 0.65f, 0.0f, 1.0f);
    return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
}

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool RowMatchesFilter(const char* filter, const char* rowText)
{
    if (!filter || !filter[0])
        return true;
    return ToLower(rowText).find(ToLower(filter)) != std::string::npos;
}

const char* GeometryFlagsStr(uint32_t flags)
{
    static char buf[8];
    int n = 0;
    if (flags & GpuGeometry::GetNormalBit())
        n += snprintf(buf + n, sizeof(buf) - n, "N ");
    if (flags & GpuGeometry::GetTangentBit())
        n += snprintf(buf + n, sizeof(buf) - n, "T ");
    if (flags & GpuGeometry::GetHasUVBit())
        n += snprintf(buf + n, sizeof(buf) - n, "U ");
    if (n == 0)
        return "-";
    if (n > 0 && buf[n - 1] == ' ')
        buf[n - 1] = '\0';
    return buf;
}

const char* SamplerDescText(int index)
{
    static const char* descs[] = {
        "Repeat + Nearest",
        "Repeat + Linear",
        "Mirror + Nearest",
        "Mirror + Linear",
        "Clamp + Nearest",
        "Clamp + Linear",
        "Border + Nearest",
        "Border + Linear",
        "Clamp + Nearest  (MirrorClamp fallback)",
        "Clamp + Linear  (MirrorClamp fallback)",
    };
    return descs[index];
}

} // namespace

bool GPUDrivenManagerDebugWindow::Tick()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(800, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("GPU Driven Manager", &open);

    auto* mgr = Rendering::GPUDrivenManager::TryGetInstance();
    if (!mgr)
    {
        ImGui::TextDisabled("GPUDrivenManager not initialized (engine not running or already shut down)");
        ImGui::End();
        return open;
    }

    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Active");
    ImGui::SameLine();
    ImGui::TextDisabled("(auto-refreshing)");
    ImGui::Separator();

    // ===== Buffer Overview =====
    if (ImGui::CollapsingHeader("Buffer Overview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        auto* globalBuf = mgr->GetGlobalBuffer();
        ImGui::Text("Global Buffer:    %p  (%u MB)", (void*)globalBuf, mgr->GetGlobalBufferSize() / (1024 * 1024));
        ImGui::Text("Shader Dev Addr:  0x%016llX", mgr->GetGlobalBufferShaderDeviceAddress());

        ImGui::Spacing();
        uint32_t arenaCap = mgr->GetIndirectCommandBufferCapacity();
        uint32_t arenaOff = mgr->GetIndirectCommandBufferOffset();
        float arenaRatio = arenaCap > 0 ? (float)arenaOff / (float)arenaCap : 0.0f;
        ImGui::Text("Indirect Arena:   %u / %u draws", arenaOff, arenaCap);
        ImGui::SameLine();
        ImGui::TextColored(UsageColor(arenaRatio), "  (%.1f%%)", arenaRatio * 100.0f);

        ImGui::Spacing();
        bool configDirty = mgr->IsGPUDrivenConfigDirty();
        ImGui::Text("Config Dirty:     ");
        ImGui::SameLine();
        ImGui::TextColored(configDirty ? ImVec4(1, 0.5f, 0, 1) : ImVec4(0.2f, 0.8f, 0.2f, 1), "%s",
                          configDirty ? "YES" : "No");
        ImGui::Unindent();
    }

    // ===== Pool Usage =====
    if (ImGui::CollapsingHeader("Pool Usage"))
    {
        static const char* poolNames[] = {"Geometry", "Material", "Object", "RenderDataList", "Texture"};
        size_t used[] = {
            mgr->GetGeometryPool().GetUsedCount(), mgr->GetMaterialPool().GetUsedCount(),
            mgr->GetObjectPool().GetUsedCount(), mgr->GetRenderDataListPool().GetUsedCount(),
            mgr->GetTexturePool().GetUsedCount()};
        size_t cap[] = {
            mgr->GetGeometryPool().GetCapacity(), mgr->GetMaterialPool().GetCapacity(),
            mgr->GetObjectPool().GetCapacity(), mgr->GetRenderDataListPool().GetCapacity(),
            mgr->GetTexturePool().GetCapacity()};

        if (ImGui::BeginTable("##PoolUsage", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Pool");
            ImGui::TableSetupColumn("Used");
            ImGui::TableSetupColumn("Capacity");
            ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int i = 0; i < 5; ++i)
            {
                ImGui::TableNextRow();
                float ratio = cap[i] > 0 ? (float)used[i] / (float)cap[i] : 0.0f;

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", poolNames[i]);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", used[i]);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu", cap[i]);

                ImGui::TableSetColumnIndex(3);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, UsageColor(ratio));
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.0f%%", ratio * 100.0f);
                ImGui::ProgressBar(ratio, ImVec2(-1, 0), overlay);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
    }

    // ===== Per-pool detail helpers =====
    auto ShowDetailHeader = [](const char* label, size_t count) -> bool {
        auto title = fmt::format("{} ({})", label, count);
        return ImGui::CollapsingHeader(title.c_str());
    };

    // ===== Geometry Descriptors =====
    {
        const auto& geomPool = mgr->GetGeometryPool();
        auto geomCount = geomPool.GetUsedCount();
        if (ShowDetailHeader("Geometry Descriptors", geomCount) && geomCount > 0)
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##geomFilter", "Filter...", filter, sizeof(filter));

            if (ImGui::BeginTable("##GeomTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                          ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, std::min((float)geomCount * ImGui::GetTextLineHeightWithSpacing() + 35,
                                                  200.0f))))
            {
                ImGui::TableSetupColumn("Handle");
                ImGui::TableSetupColumn("IdxCount");
                ImGui::TableSetupColumn("IdxOff");
                ImGui::TableSetupColumn("PosOff");
                ImGui::TableSetupColumn("AttrOff");
                ImGui::TableSetupColumn("Stride");
                ImGui::TableSetupColumn("Flags");
                ImGui::TableHeadersRow();

                for (auto it = geomPool.cbegin(); it != geomPool.cend(); ++it)
                {
                    auto& desc = *it;
                    auto rowText = fmt::format("{} {} {} {} {} {} {}", it.index, desc.geometry.indexCount,
                                               desc.geometry.indexOffset, desc.geometry.positionOffset,
                                               desc.geometry.attributeOffset, desc.geometry.attributeStride,
                                               GeometryFlagsStr(desc.geometry.attributeFlags));
                    if (!RowMatchesFilter(filter, rowText.c_str()))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", it.index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", desc.geometry.indexCount);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("0x%X", desc.geometry.indexOffset);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("0x%X", desc.geometry.positionOffset);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("0x%X", desc.geometry.attributeOffset);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%u", desc.geometry.attributeStride);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%s", GeometryFlagsStr(desc.geometry.attributeFlags));
                }
                ImGui::EndTable();
            }
        }
    }

    // ===== Material Descriptors =====
    {
        const auto& matPool = mgr->GetMaterialPool();
        auto matCount = matPool.GetUsedCount();
        if (ShowDetailHeader("Material Descriptors", matCount) && matCount > 0)
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##matFilter", "Filter...", filter, sizeof(filter));

            if (ImGui::BeginTable("##MatTable", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                          ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, std::min((float)matCount * ImGui::GetTextLineHeightWithSpacing() + 35,
                                                  200.0f))))
            {
                ImGui::TableSetupColumn("Handle");
                ImGui::TableSetupColumn("BaseColor");
                ImGui::TableSetupColumn("Roughness");
                ImGui::TableSetupColumn("Metallic");
                ImGui::TableSetupColumn("AlphaCut");
                ImGui::TableSetupColumn("Emissive");
                ImGui::TableSetupColumn("ShaderHash");
                ImGui::TableSetupColumn("BaseTex");
                ImGui::TableSetupColumn("NormTex");
                ImGui::TableSetupColumn("MRTex");
                ImGui::TableHeadersRow();

                for (auto it = matPool.cbegin(); it != matPool.cend(); ++it)
                {
                    auto& desc = *it;
                    auto& m = desc.materialData;
                    auto rowText = fmt::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:X} {} {} {} {}",
                                               it.index, m.baseColorFactor.r, m.baseColorFactor.g,
                                               m.baseColorFactor.b, m.baseColorFactor.a, m.roughness, m.metallic,
                                               m.alphaCutoff, m.emissive.r, m.emissive.g, m.emissive.b, m.emissive.a,
                                               m.shaderHash, m.baseColorTexIndex.x, m.normalMapTexIndex.x,
                                               m.metallicRoughnessTexIndex.x, m.emissiveMapTexIndex.x);
                    if (!RowMatchesFilter(filter, rowText.c_str()))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", it.index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f %.2f %.2f %.2f", (double)m.baseColorFactor.r, (double)m.baseColorFactor.g,
                                (double)m.baseColorFactor.b, (double)m.baseColorFactor.a);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.3f", (double)m.roughness);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.3f", (double)m.metallic);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.3f", (double)m.alphaCutoff);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%.2f %.2f %.2f", (double)m.emissive.r, (double)m.emissive.g, (double)m.emissive.b);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("0x%X", m.shaderHash);
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%u", m.baseColorTexIndex.x);
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%u", m.normalMapTexIndex.x);
                    ImGui::TableSetColumnIndex(9);
                    ImGui::Text("%u", m.metallicRoughnessTexIndex.x);
                }
                ImGui::EndTable();
            }
        }
    }

    // ===== Object Descriptors =====
    {
        const auto& objPool = mgr->GetObjectPool();
        auto objCount = objPool.GetUsedCount();
        if (ShowDetailHeader("Object Descriptors", objCount) && objCount > 0)
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##objFilter", "Filter...", filter, sizeof(filter));

            if (ImGui::BeginTable("##ObjTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, std::min((float)objCount * ImGui::GetTextLineHeightWithSpacing() + 35,
                                                  200.0f))))
            {
                ImGui::TableSetupColumn("Handle");
                ImGui::TableSetupColumn("RdCount");
                ImGui::TableSetupColumn("RdOffset");
                ImGui::TableSetupColumn("RdListHnd");
                ImGui::TableSetupColumn("Model");
                ImGui::TableHeadersRow();

                for (auto it = objPool.cbegin(); it != objPool.cend(); ++it)
                {
                    auto& desc = *it;
                    auto& o = desc.gpuObject;
                    auto rowText =
                        fmt::format("{} {} {} {}", it.index, o.renderDataCount, o.pRenderDataOffset,
                                    (uint64_t)desc.renderDataListHandle);
                    if (!RowMatchesFilter(filter, rowText.c_str()))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", it.index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", o.renderDataCount);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("0x%X", o.pRenderDataOffset);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%llu", (uint64_t)desc.renderDataListHandle);

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("[c0:% .2f % .2f % .2f % .2f]", (double)o.model[0].x, (double)o.model[0].y,
                                (double)o.model[0].z, (double)o.model[0].w);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Model Matrix:");
                        for (int r = 0; r < 4; ++r)
                            ImGui::Text("  [% .2f % .2f % .2f % .2f]", (double)o.model[r].x, (double)o.model[r].y,
                                        (double)o.model[r].z, (double)o.model[r].w);
                        ImGui::Separator();
                        ImGui::Text("InvTsp Matrix:");
                        for (int r = 0; r < 4; ++r)
                            ImGui::Text("  [% .2f % .2f % .2f % .2f]", (double)o.invTspModel[r].x,
                                        (double)o.invTspModel[r].y, (double)o.invTspModel[r].z,
                                        (double)o.invTspModel[r].w);
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    // ===== Render Data Lists =====
    {
        const auto& rdPool = mgr->GetRenderDataListPool();
        auto rdCount = rdPool.GetUsedCount();
        if (ShowDetailHeader("Render Data Lists", rdCount) && rdCount > 0)
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##rdFilter", "Filter...", filter, sizeof(filter));

            for (auto it = rdPool.cbegin(); it != rdPool.cend(); ++it)
            {
                auto& desc = *it;
                auto rowText =
                    fmt::format("{} {}", it.index, desc.renderDataList.size());
                if (!RowMatchesFilter(filter, rowText.c_str()))
                    continue;

                auto label =
                    fmt::format("[{}]  {} entries  (alloc: offset=0x{:X} size={})", it.index,
                                desc.renderDataList.size(), desc.dataAlloc.offset, desc.dataAlloc.size);
                ImGuiTreeNodeFlags treeFlags = desc.renderDataList.empty() ? ImGuiTreeNodeFlags_Leaf : 0;
                if (ImGui::TreeNodeEx(label.c_str(), treeFlags))
                {
                    if (!desc.renderDataList.empty())
                    {
                        if (ImGui::BeginTable("##rdEntries", 3,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_ScrollY,
                                              ImVec2(0, std::min((float)desc.renderDataList.size() *
                                                                      ImGui::GetTextLineHeightWithSpacing() +
                                                                  35,
                                                              150.0f))))
                        {
                            ImGui::TableSetupColumn("Idx");
                            ImGui::TableSetupColumn("GeomHnd");
                            ImGui::TableSetupColumn("MatHnd");
                            ImGui::TableHeadersRow();

                            for (size_t i = 0; i < desc.renderDataList.size(); ++i)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%zu", i);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%llu", (uint64_t)desc.renderDataHandles[i].geometryHandle);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%llu", (uint64_t)desc.renderDataHandles[i].materialHandle);
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    // ===== Texture Slots =====
    {
        const auto& texPool = mgr->GetTexturePool();
        auto texCount = texPool.GetUsedCount();
        if (ShowDetailHeader("Texture Slots", texCount) && texCount > 0)
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##texFilter", "Filter...", filter, sizeof(filter));

            if (ImGui::BeginTable("##TexTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                       ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, std::min((float)texCount * ImGui::GetTextLineHeightWithSpacing() + 35,
                                                  200.0f))))
            {
                ImGui::TableSetupColumn("Handle");
                ImGui::TableSetupColumn("Texture Ptr");
                ImGui::TableSetupColumn("Texture Name");
                ImGui::TableHeadersRow();

                for (auto it = texPool.cbegin(); it != texPool.cend(); ++it)
                {
                    auto& slot = *it;
                    auto* tex = slot.texture;
                    auto rowText = fmt::format("{} {} {}", it.index, (void*)tex,
                                               tex ? tex->GetName() : "");
                    if (!RowMatchesFilter(filter, rowText.c_str()))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", it.index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%p", (void*)tex);
                    ImGui::TableSetColumnIndex(2);
                    if (tex)
                        ImGui::Text("%s", tex->GetName().c_str());
                    else
                        ImGui::TextDisabled("(null)");
                }
                ImGui::EndTable();
            }
        }
    }

    // ===== Uniform Buffers =====
    if (ImGui::CollapsingHeader("Uniform & Config Buffers"))
    {
        if (ImGui::BeginTable("##UniformBuf", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Buffer");
            ImGui::TableSetupColumn("Pointer");
            ImGui::TableHeadersRow();

            auto BufferRow = [](const char* name, Gfx::Buffer* buf) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", name);
                ImGui::TableSetColumnIndex(1);
                if (buf)
                    ImGui::Text("%p", (void*)buf);
                else
                    ImGui::TextDisabled("(null)");
            };

            BufferRow("Scene", mgr->GetSceneBuffer());
            BufferRow("Camera", mgr->GetCameraBuffer());
            BufferRow("MainLightShadow", mgr->GetMainLightShadowBuffer());
            BufferRow("Config", mgr->GetGPUDrivenConfigBuffer());
            BufferRow("Indirect Cmd", mgr->GetIndirectCommandBuffer());
            BufferRow("Indirect Extra", mgr->GetIndirectCommandExtraBuffer());

            ImGui::EndTable();
        }
    }

    // ===== Samplers =====
    if (ImGui::CollapsingHeader("Samplers (10)"))
    {
        if (ImGui::BeginTable("##SamplerTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Description");
            ImGui::TableHeadersRow();

            for (int i = 0; i < 10; ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("[%d]", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", SamplerDescText(i));
            }
            ImGui::EndTable();
        }
    }

    // ===== Indirect Draw Arena Details =====
    if (ImGui::CollapsingHeader("Indirect Draw Arena"))
    {
        ImGui::Indent();
        ImGui::Text("Arena Frame Index:  %llu", mgr->GetIndirectArenaFrameIndex());
        ImGui::Text("Buffer Capacity:    %u draws", mgr->GetIndirectCommandBufferCapacity());
        ImGui::Text("Current Offset:     %u draws", mgr->GetIndirectCommandBufferOffset());

        uint32_t cap = mgr->GetIndirectCommandBufferCapacity();
        if (cap > 0)
        {
            float usedGiB = (float)(mgr->GetIndirectCommandBufferOffset() * sizeof(DrawIndexedIndirectCommand)) /
                            (1024.0f * 1024.0f * 1024.0f);
            float totalGiB = (float)(cap * sizeof(DrawIndexedIndirectCommand)) / (1024.0f * 1024.0f * 1024.0f);
            ImGui::Text("Memory:             %.4f GiB / %.4f GiB", (double)usedGiB, (double)totalGiB);
        }
        ImGui::Unindent();
    }

    ImGui::End();
    return open;
}

} // namespace Editor
