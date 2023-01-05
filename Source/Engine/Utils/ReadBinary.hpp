#pragma once
#include <fstream>
#include <string>
#include <filesystem>
#include "Libs/Ptr.hpp"

namespace Engine
{
    UniPtr<char> ReadBinary(const std::filesystem::path& path, size_t* byteSize = nullptr)
    {
        // read file
        std::fstream f;
        f.open(path, std::ios::in | std::ios::binary);
        if (!f.is_open() || !f.good()) return nullptr;

        f.seekg(0, std::ios::end);
        size_t fSize = std::filesystem::file_size(path);
        UniPtr<char> buf = UniPtr<char>(new char[fSize]);
        // shader Buffer
        f.seekg(0, std::ios::beg);
        f.read(buf.Get(), fSize);

        if (byteSize != nullptr) *byteSize = fSize;
        return buf;
    }
}
