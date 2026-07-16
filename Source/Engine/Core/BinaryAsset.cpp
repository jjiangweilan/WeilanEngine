#include "BinaryAsset.hpp"

#include <fstream>
#include <iterator>

DEFINE_ASSET(BinaryAsset, "7B623E1C-8BE5-4C45-8AFF-3A84BB8C6267", "bin")

bool BinaryAsset::LoadFromFile(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open() || !input.good())
        return false;

    data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

bool BinaryAsset::SaveToFile(const std::filesystem::path& path) const
{
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open() || !output.good())
        return false;

    if (!data.empty())
        output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    return output.good();
}
