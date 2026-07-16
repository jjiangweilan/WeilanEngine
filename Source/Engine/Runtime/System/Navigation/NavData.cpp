#include "NavData.hpp"

#include "Engine/Core/BinaryAsset.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <limits>
#include <spdlog/spdlog.h>

DEFINE_ASSET(NavData, "AF102612-B343-45BB-B7BC-540B2EA2F713", "nav")

DEFINE_SERIALIZATION(NavData, Asset, SER(grid), SER(cellAsset))

namespace
{
constexpr uint8_t NavCellMagic[] = {'W', 'N', 'A', 'V'};
constexpr uint32_t NavCellFormatVersion = 1;
constexpr uint32_t NavCellRecordSize = 24;
constexpr size_t NavCellHeaderSize = 16;

void AppendUInt32(std::vector<uint8_t>& output, uint32_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
    output.push_back(static_cast<uint8_t>(value >> 16));
    output.push_back(static_cast<uint8_t>(value >> 24));
}

void AppendFloat(std::vector<uint8_t>& output, float value)
{
    AppendUInt32(output, std::bit_cast<uint32_t>(value));
}

uint32_t ReadUInt32(const std::vector<uint8_t>& input, size_t& offset)
{
    uint32_t value = static_cast<uint32_t>(input[offset]) |
                     (static_cast<uint32_t>(input[offset + 1]) << 8) |
                     (static_cast<uint32_t>(input[offset + 2]) << 16) |
                     (static_cast<uint32_t>(input[offset + 3]) << 24);
    offset += sizeof(uint32_t);
    return value;
}

float ReadFloat(const std::vector<uint8_t>& input, size_t& offset)
{
    return std::bit_cast<float>(ReadUInt32(input, offset));
}
} // namespace

BinaryAsset* NavData::GetCellAsset() const
{
    return cellAsset.Get();
}

void NavData::SetCellAsset(BinaryAsset* asset)
{
    cellAsset = asset;
    SetDirty();
}

void NavData::ClearCells()
{
    grid.cells.clear();
    WriteCellsToBinary();
    SetDirty();
}

bool NavData::WriteCellsToBinary()
{
    BinaryAsset* binaryAsset = cellAsset.Get();
    if (binaryAsset == nullptr)
        return false;

    if (grid.cells.size() > std::numeric_limits<uint32_t>::max())
    {
        spdlog::error("NavData {} has too many cells for the binary format", GetName());
        return false;
    }

    std::vector<uint8_t> data;
    data.reserve(NavCellHeaderSize + grid.cells.size() * NavCellRecordSize);
    data.insert(data.end(), std::begin(NavCellMagic), std::end(NavCellMagic));
    AppendUInt32(data, NavCellFormatVersion);
    AppendUInt32(data, NavCellRecordSize);
    AppendUInt32(data, static_cast<uint32_t>(grid.cells.size()));

    for (const NavCell& cell : grid.cells)
    {
        AppendFloat(data, cell.height);
        AppendFloat(data, cell.edgeSlop.x);
        AppendFloat(data, cell.edgeSlop.y);
        AppendFloat(data, cell.edgeSlop.z);
        AppendFloat(data, cell.edgeSlop.w);
        data.push_back(cell.valid ? 1 : 0);
        data.insert(data.end(), 3, 0);
    }

    binaryAsset->SetData(std::move(data));
    return true;
}

bool NavData::ReadCellsFromBinary()
{
    BinaryAsset* binaryAsset = cellAsset.Get();
    if (binaryAsset == nullptr || binaryAsset->GetData().empty())
    {
        grid.cells.clear();
        return false;
    }

    const std::vector<uint8_t>& data = binaryAsset->GetData();
    auto reject = [this](const char* reason)
    {
        grid.cells.clear();
        spdlog::warn("failed to load NavData {} cells: {}", GetName(), reason);
        return false;
    };

    if (data.size() < NavCellHeaderSize || !std::equal(std::begin(NavCellMagic), std::end(NavCellMagic), data.begin()))
        return reject("invalid header");

    size_t offset = sizeof(NavCellMagic);
    const uint32_t version = ReadUInt32(data, offset);
    const uint32_t recordSize = ReadUInt32(data, offset);
    const uint32_t cellCount = ReadUInt32(data, offset);

    if (version != NavCellFormatVersion)
        return reject("unsupported version");
    if (recordSize != NavCellRecordSize)
        return reject("invalid record size");

    const uint64_t expectedDataSize = NavCellHeaderSize + static_cast<uint64_t>(cellCount) * recordSize;
    if (expectedDataSize != data.size())
        return reject("payload size mismatch");

    if (cellCount != 0)
    {
        if (grid.config.width <= 0 || grid.config.height <= 0)
            return reject("invalid grid dimensions");

        const uint64_t expectedCellCount =
            static_cast<uint64_t>(grid.config.width) * static_cast<uint64_t>(grid.config.height);
        if (cellCount != expectedCellCount)
            return reject("cell count does not match grid dimensions");
    }

    std::vector<NavCell> cells(cellCount);
    for (NavCell& cell : cells)
    {
        cell.height = ReadFloat(data, offset);
        cell.edgeSlop.x = ReadFloat(data, offset);
        cell.edgeSlop.y = ReadFloat(data, offset);
        cell.edgeSlop.z = ReadFloat(data, offset);
        cell.edgeSlop.w = ReadFloat(data, offset);

        const uint8_t valid = data[offset];
        if (valid > 1)
            return reject("invalid cell validity value");
        cell.valid = valid != 0;
        offset += 4;
    }

    grid.cells = std::move(cells);
    return true;
}

void NavData::OnLoaded()
{
    ReadCellsFromBinary();
}
