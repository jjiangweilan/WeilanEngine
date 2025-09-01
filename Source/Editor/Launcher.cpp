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
    Launcher l;
    l.ArgsParser(argc, argv);
}
