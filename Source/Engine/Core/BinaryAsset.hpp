#pragma once

#include "Asset.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class WEILAN_ENGINE_API BinaryAsset final : public Asset
{
    DECLARE_ASSET();

public:
    BinaryAsset() = default;
    explicit BinaryAsset(std::vector<uint8_t> data) : data(std::move(data)) {}

    const std::vector<uint8_t>& GetData() const { return data; }
    size_t GetSize() const { return data.size(); }

    void SetData(std::vector<uint8_t> newData)
    {
        data = std::move(newData);
        SetDirty();
    }

    void ClearData() { SetData({}); }

    bool LoadFromFile(const char* path) override;
    bool SaveToFile(const std::filesystem::path& path) const override;

private:
    std::vector<uint8_t> data;
};
