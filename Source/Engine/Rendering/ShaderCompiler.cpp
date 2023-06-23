#include "ShaderCompiler.hpp"

namespace Engine
{
shaderc_include_result* ShaderCompiler::ShaderIncluder::GetInclude(
    const char* requested_source,
    shaderc_include_type type,
    const char* requesting_source,
    size_t include_depth)
{
    std::filesystem::path requestingSource(requesting_source);
    if (include_depth == 1)
    {
        requestingSourceRelativeFullPath = "Assets/Shaders/";
    }
    else
    {
        requestingSourceRelativeFullPath /= requestingSource.parent_path();
    }

    // if the we can't find the included file in the project, try find it in
    // the internal assets
    std::filesystem::path relativePath =
        requestingSourceRelativeFullPath /
        std::filesystem::path(requested_source);
    // #if GAME_EDITOR
    //             if (!std::filesystem::exists(relativePath))
    //             {
    //                 std::filesystem::path enginePath =
    //                 Editor::ProjectManagement::GetInternalRootPath();
    //                 relativePath = enginePath / relativePath;
    //             }
    // #endif

    std::fstream f;
    f.open(relativePath, std::ios::in);
    if (f.fail())
    {
        return new shaderc_include_result{"",
                                          0,
                                          "Can't find requested shader file."};
    }
    if (includedFiles)
        includedFiles->insert(std::filesystem::absolute(relativePath));

    FileData* fileData = new FileData();
    fileData->sourceName = std::string(requested_source);
    fileData->content = std::vector<char>(std::istreambuf_iterator<char>(f),
                                          std::istreambuf_iterator<char>());

    shaderc_include_result* result = new shaderc_include_result;
    result->content = fileData->content.data();
    result->content_length = fileData->content.size();
    result->source_name = fileData->sourceName.data();
    result->source_name_length = fileData->sourceName.size();
    result->user_data = fileData;
    return result;
}
} // namespace Engine
