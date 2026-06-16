#include "Editor/GameEditor_AssetDatabaseDebug.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace Editor
{
namespace
{
template <class TObjectMap>
static std::vector<GameEditorAssetDatabaseDebug::EngineObjectInfo> CollectEngineObjects(const TObjectMap& loadedObjects)
{
    std::vector<GameEditorAssetDatabaseDebug::EngineObjectInfo> objects;
    for (auto& [uuid, object] : loadedObjects)
    {
        objects.push_back({uuid, ObjPtr<Object>(object), ObjectRegistry::GetObjectTypeInfo(object->GetObjectTypeID())->GetTypeName()});
    }

    std::sort(objects.begin(), objects.end(), [](const GameEditorAssetDatabaseDebug::EngineObjectInfo& l, const GameEditorAssetDatabaseDebug::EngineObjectInfo& r)
              { return l.typeName < r.typeName; });
    return objects;
}

static int CountAliveObjects(const std::vector<GameEditorAssetDatabaseDebug::EngineObjectInfo>& engineObjects)
{
    int alive = 0;
    for (const GameEditorAssetDatabaseDebug::EngineObjectInfo& info : engineObjects)
    {
        if (info.object.Get() != nullptr)
        {
            ++alive;
        }
    }
    return alive;
}

template <class TObjectMap>
static std::vector<GameEditorAssetDatabaseDebug::AssetDataInfo> CollectAssetDataInfos(const TObjectMap& loadedObjects)
{
    std::vector<GameEditorAssetDatabaseDebug::AssetDataInfo> infos;
    for (auto& data : AssetDatabase::Singleton()->GetAssetData())
    {
        const UUID& assetUUID = data->GetAssetUUID();
        infos.push_back({
            data.get(),
            assetUUID,
            data->GetAssetPath(),
            assetUUID.ToString(),
            loadedObjects.find(assetUUID) != loadedObjects.end(),
            std::filesystem::exists(data->GetAssetAbsolutePath()) || !data->IsValid()});
    }

    std::sort(infos.begin(), infos.end(), [](const GameEditorAssetDatabaseDebug::AssetDataInfo& l, const GameEditorAssetDatabaseDebug::AssetDataInfo& r)
              { return l.path.string() < r.path.string(); });
    return infos;
}

static GameEditorAssetDatabaseDebug::DuplicateUUIDMap ScanDuplicateUUIDs(const std::vector<GameEditorAssetDatabaseDebug::AssetDataInfo>& assetDatas)
{
    GameEditorAssetDatabaseDebug::DuplicateUUIDMap uuids;
    for (const GameEditorAssetDatabaseDebug::AssetDataInfo& info : assetDatas)
    {
        if (!info.assetUUID.IsEmpty())
        {
            uuids[info.uuidText].push_back({"Asset", info.path.string(), info.path.GetFileName()});
        }

        for (const auto& [name, uuid] : info.data->GetInternalObjectAssetNameToUUID())
        {
            if (!uuid.IsEmpty())
            {
                uuids[uuid.ToString()].push_back({"Internal Object", info.path.string(), name});
            }
        }
    }

    for (auto iter = uuids.begin(); iter != uuids.end();)
    {
        if (iter->second.size() < 2)
        {
            iter = uuids.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    return uuids;
}

static int CountMissingFiles(const std::vector<GameEditorAssetDatabaseDebug::AssetDataInfo>& assetDatas)
{
    int missing = 0;
    for (const GameEditorAssetDatabaseDebug::AssetDataInfo& info : assetDatas)
    {
        if (!info.fileExists)
        {
            ++missing;
        }
    }
    return missing;
}

static void DrawMetric(const char* label, int value)
{
    ImGui::TextDisabled("%s", label);
    ImGui::Text("%d", value);
}
} // namespace

void GameEditorAssetDatabaseDebug::DrawSummary()
{
    if (ImGui::BeginTable("AssetDatabaseDebugSummary", 5, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawMetric("AssetData", static_cast<int>(assetDatas.size()));

        ImGui::TableSetColumnIndex(1);
        DrawMetric("Cached Objects", static_cast<int>(engineObjects.size()));

        ImGui::TableSetColumnIndex(2);
        DrawMetric("Alive Objects", CountAliveObjects(engineObjects));

        ImGui::TableSetColumnIndex(3);
        DrawMetric("Missing Files", CountMissingFiles(assetDatas));

        ImGui::TableSetColumnIndex(4);
        DrawMetric("Duplicate UUIDs", static_cast<int>(duplicateUUIDs.size()));

        ImGui::EndTable();
    }
}

void GameEditorAssetDatabaseDebug::DrawDuplicateUUIDReport()
{
    ImGui::SeparatorText("Utilities");
    ImGui::TextWrapped("Scan all AssetData entries and their internal object UUID maps for duplicate UUIDs.");

    if (!duplicateScanRan)
    {
        ImGui::TextDisabled("Run Scan Duplicate UUIDs to populate this report.");
        return;
    }

    if (duplicateUUIDs.empty())
    {
        ImGui::TextColored(ImVec4{0.3f, 0.9f, 0.3f, 1.0f}, "No duplicate UUIDs found.");
        return;
    }

    ImGui::TextColored(ImVec4{1.0f, 0.45f, 0.25f, 1.0f}, "%d duplicated UUID group(s) found.", static_cast<int>(duplicateUUIDs.size()));
    if (ImGui::BeginTable(
            "DuplicateUUIDTable",
            4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 220)
        ))
    {
        ImGui::TableSetupColumn("UUID");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Label");
        ImGui::TableHeadersRow();

        std::vector<const std::pair<const std::string, std::vector<DuplicateUUIDEntry>>*> rows;
        for (const auto& group : duplicateUUIDs)
        {
            rows.push_back(&group);
        }
        std::sort(rows.begin(), rows.end(), [](const auto* l, const auto* r)
                  { return l->first < r->first; });

        for (const auto* group : rows)
        {
            for (const DuplicateUUIDEntry& entry : group->second)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(group->first.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.kind.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(entry.path.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(entry.label.c_str());
            }
        }

        ImGui::EndTable();
    }
}

void GameEditorAssetDatabaseDebug::DrawAssetDataTable()
{
    ImGui::SeparatorText("Asset Data");
    if (ImGui::BeginTable(
            "AssetDataDebugTable",
            6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300)
        ))
    {
        ImGui::TableSetupColumn("File");
        ImGui::TableSetupColumn("UUID");
        ImGui::TableSetupColumn("Loaded");
        ImGui::TableSetupColumn("File Exists");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        int uid = 0;
        for (const AssetDataInfo& info : assetDatas)
        {
            ImGui::PushID(uid++);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(info.path.GetFileName().c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.uuidText.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(info.loaded ? ImVec4{0, 1, 0, 1} : ImVec4{1, 0, 0, 1}, "%s", info.loaded ? "true" : "false");

            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(info.fileExists ? ImVec4{0, 1, 0, 1} : ImVec4{1, 0, 0, 1}, "%s", info.fileExists ? "true" : "false");

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(info.path.string().c_str());

            ImGui::TableSetColumnIndex(5);
            if (ImGui::SmallButton("Delete AssetData"))
            {
                AssetDatabase::Singleton()->RemoveAssetData(info.data);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void GameEditorAssetDatabaseDebug::DrawEngineObjectTable()
{
    ImGui::SeparatorText("Engine Objects");
    if (ImGui::BeginTable(
            "EngineObjectDebugTable",
            5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300)
        ))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Alive");
        ImGui::TableSetupColumn("UUID");
        ImGui::TableSetupColumn("Asset Path");
        ImGui::TableHeadersRow();

        for (const EngineObjectInfo& info : engineObjects)
        {
            ImGui::TableNextRow();
            std::string uuid = info.uuid.ToString();
            Object* object = info.object.Get();
            const bool alive = object != nullptr;
            const std::string& name = alive ? object->GetName() : uuid;

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.typeName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(alive ? ImVec4{0, 1, 0, 1} : ImVec4{1, 0, 0, 1}, "%s", alive ? "true" : "false");

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(uuid.c_str());

            ImGui::TableSetColumnIndex(4);
            if (Asset* asAsset = dynamic_cast<Asset*>(object))
            {
                ImGui::TextUnformatted(AssetDatabase::Singleton()->GetAssetPath(asAsset->GetUUID()).string().c_str());
            }
        }

        ImGui::EndTable();
    }
}

void GameEditorAssetDatabaseDebug::RefreshCache()
{
    loadedObjects = Object::GetAllEngineObjects();
    engineObjects = CollectEngineObjects(loadedObjects);
    assetDatas = CollectAssetDataInfos(loadedObjects);
    duplicateUUIDs.clear();
    duplicateScanRan = false;
    cacheBuilt = true;
}

void GameEditorAssetDatabaseDebug::Show(bool& open)
{
    if (!open)
        return;

    if (!ImGui::Begin("AssetDatabase", &open))
    {
        ImGui::End();
        return;
    }

    if (!cacheBuilt)
    {
        RefreshCache();
    }

    DrawSummary();

    if (ImGui::Button("Refresh"))
    {
        RefreshCache();
    }

    ImGui::SameLine();

    if (ImGui::Button("Scan Duplicate UUIDs"))
    {
        RefreshCache(); // ensure we have the latest data before scanning
        duplicateUUIDs = ScanDuplicateUUIDs(assetDatas);
        duplicateScanRan = true;
    }

    DrawDuplicateUUIDReport();
    DrawAssetDataTable();
    DrawEngineObjectTable();

    ImGui::End();
}
} // namespace Editor
