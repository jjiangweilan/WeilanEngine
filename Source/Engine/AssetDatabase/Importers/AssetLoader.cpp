#include "AssetLoader.hpp"
#include <fstream>

ImportDatabase& ImportDatabase::Singleton()
{
    static ImportDatabase importDatabase;
    return importDatabase;
}

std::vector<uint8_t> ImportDatabase::ReadFile(const std::string& filename)
{
    std::fstream f;
    f.open(filename, std::ios::binary | std::ios_base::in);
    if (f.good() && f.is_open())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        std::string s = ss.str();
        std::vector<uint8_t> d(s.size());
        memcpy(d.data(), s.data(), s.size());

        return d;
    }
    return {};
}
