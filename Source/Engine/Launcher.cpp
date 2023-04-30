#include "WeilanEngine.hpp"
// test
#include "Core/Resource.hpp"
#include <iostream>
// test

#undef main

namespace Engine
{
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
            if (std::filesystem::exists(path))
            {
                OpenProject(path);
            }
            else
            {
                SPDLOG_ERROR("{} is not a valid path", path.string());
            }

            hasAction = true;
        }
    }

    // I want to keep the data on the stack at minimum before I actually launch the engine
    // this function should be called before launcher launches the game

    void OpenProject(const std::filesystem::path& path)
    {
        Engine::WeilanEngine backend;
        backend.Launch(path);
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

} // namespace Engine
  //
  //

int main(int argc, char** argv)
{
    Engine::Launcher l;
    l.ArgsParser(argc, argv);
}
