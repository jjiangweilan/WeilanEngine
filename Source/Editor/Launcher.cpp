#include "GameEditor.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

class Launcher
{
    using ArgList = std::vector<std::string_view>;

public:
    Launcher() {}

    void ArgsParser(int argc, char** argv)
    {
        argList = std::make_unique<ArgList>(argv + 1, argv + argc);
        for (int i = 0; i < argList->size(); ++i)
        {
            DispatchArgs(*argList, i);
        }

        if (!hasAction)
        {
            SPDLOG_ERROR("No action taken, maybe you should set a project path using --project");
        }
    }

    void DispatchArgs(ArgList& args, int& curr)
    {
        if (args[curr] == "--project" || args[curr] == "-p")
        {
            curr++;
            std::filesystem::path path(args[curr]);
            // if (std::filesystem::exists(path))
            // {}
            // else
            // {
            //     SPDLOG_ERROR("{} is not a valid path", path.string());
            // }

            auto editor = std::make_unique<Editor::GameEditor>(path.string().c_str());
            editor->Start();

            hasAction = true;
        }
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

#undef main

int main(int argc, char** argv)
{
    glm::vec3 position{ 0, 0, 2.0f };
    glm::quat rotation{1,0,0,0};
    glm::vec3 scale{ 1,1,1 };

    glm::mat4 modelMatrix =
        glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
    glm::mat4 viewMatrix = glm::inverse(modelMatrix);
    glm::mat4 viewMatrix0 =
        glm::inverse(glm::translate(glm::mat4(1), glm::vec3{0,0,0.0}) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1080.0f / 1920.0f, 0.01f, 1000.0f);
    proj[1][1] *= -1;
    glm::vec4 ss = proj * viewMatrix * glm::vec4(0, 0, 5.0f, 1.0);
    glm::vec4 ss0 = proj * viewMatrix0 * glm::vec4(0, 0, 5.0f, 1.0);
    ss /= ss.w;
    ss0 /= ss0.w;
    std::cout << fmt::format("{}, {}, {}, {}", ss.x, ss.y, ss.z, ss.w) << std::endl;
    std::cout << fmt::format("{}, {}, {}, {}", ss0.x, ss0.y, ss0.z, ss0.w);
    return 0;
    Launcher l;
    l.ArgsParser(argc, argv);
}
