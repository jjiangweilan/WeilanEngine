#include "AssetLoader.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <spdlog/spdlog.h>
namespace Engine
{
    AssetsLoader::AssetsLoader()
    {
        rootPath = std::filesystem::current_path().string() + "/";
    }

    AssetsLoader::~AssetsLoader()
    {

    }

    AssetsLoader* AssetsLoader::Instance()
    {
        if (instance == nullptr)
        {
            instance = new AssetsLoader();
        }

        return instance;
    }

    std::vector<unsigned char> AssetsLoader::ReadAsBinary(const std::string& path)
    {
        std::error_code errorCode;
        uintmax_t fileSize = std::filesystem::file_size(path, errorCode);

        std::vector<unsigned char> fileContent;

        if (errorCode.value() != 0)
        {
            SPDLOG_ERROR("Failed to get file size. {} File path: {}", errorCode.message(), path);
            return fileContent;
        }

        fileContent.resize(fileSize);

        std::ifstream fs;
        fs.open(path, std::ios::in | std::ios::binary);

        fs.read(reinterpret_cast<char*>(&fileContent[0]), fileSize);

        return fileContent;
    }

    auto AssetsLoader::GetAllPathIn(const char* path) -> decltype(std::filesystem::recursive_directory_iterator(path))
    {
        return std::filesystem::recursive_directory_iterator(rootPath+path);
    }

    nlohmann::json AssetsLoader::ReadJson(const std::string& path)
    {
        std::ifstream ifs;
        ifs.open("Assets/" + path);
        if (ifs.is_open())
        {
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            nlohmann::json rlt = nlohmann::json::parse(buffer.str());
            ifs.close();
            return rlt;
        }

        return nlohmann::json();
    }

    AssetsLoader* AssetsLoader::instance = nullptr;
}
