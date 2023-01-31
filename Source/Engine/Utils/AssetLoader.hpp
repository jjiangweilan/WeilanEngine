#include "nlohmann/json.hpp"
#include <filesystem>
#include <string>
namespace Engine
{
class AssetsLoader
{
public:
    ~AssetsLoader();

    std::vector<unsigned char> ReadAsBinary(const std::string& path);
    nlohmann::json ReadJson(const std::string& path);

    auto GetAllPathIn(const char* path) -> decltype(std::filesystem::recursive_directory_iterator(path));
    const std::string& GetRootPath() { return rootPath; }

private:
    AssetsLoader();

    std::string rootPath;

public:
    static AssetsLoader* Instance();

private:
    static AssetsLoader* instance;
};
} // namespace Engine
