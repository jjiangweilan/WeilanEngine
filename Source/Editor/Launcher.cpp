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

glm::mat4 SetProjectionMatrix(float fovy, float aspect, float n, float f)
{

    float tangent = glm::tan(fovy / 2);
    float t = n * tangent;
    float r = t * aspect;

    glm::mat4 proj(0.0f);
    proj[0][0] = n / r;
    proj[1][1] = - n / t;
    proj[2][2] = f / (f - n);
    proj[2][3] = 1;
    proj[3][2] = - f * n / (f - n);
    return proj;
}

int main(int argc, char** argv)
{
     //glm::vec3 cameraPos{ 0, 0, 2.0f };
     //glm::quat rotation{1,0,0,0};
     //glm::vec3 scale{ 1,1,1 };
    
     //glm::mat4 modelMatrix =
     //    glm::translate(glm::mat4(1), cameraPos) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
     //glm::mat4 viewMatrix = glm::inverse(modelMatrix);
     //glm::mat4 proj = SetProjectionMatrix(glm::radians(60.0f), 1920.0f / 1080.0f, 0.1f, 1000.0f);
     //glm::vec4 ss = proj * viewMatrix * glm::vec4(5, 5, 5.0f, 1.0);
     //ss /= ss.w;
     //std::cout << fmt::format("{}, {}, {}, {}", ss.x, ss.y, ss.z, ss.w) << std::endl;
     //return 0;
    Launcher l;
    l.ArgsParser(argc, argv);
}
