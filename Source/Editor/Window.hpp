#pragma once
#include <functional>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace Editor
{

class Window
{
public:
    virtual bool Tick() = 0;
    virtual void OnClose(){};

private:
};

struct WindowRegisterInfo
{
    std::function<std::unique_ptr<Window>()> factory;
    std::vector<std::string> menuPath;
};

class WindowRegistery
{
public:
    template <class T>
    static bool Register(const std::string& menuPath);
    static std::span<WindowRegisterInfo> GetRegistery()
    {
        return GetSingleton().registery;
    }

private:
    static WindowRegistery& GetSingleton();

    std::vector<WindowRegisterInfo> registery;
};

// menuPath: / separated string
template <class T>
bool WindowRegistery::Register(const std::string& menuPath)
{
    std::stringstream ss(menuPath);
    std::vector<std::string> pathTokens;
    std::string token;
    char delimiter = '/';
    while (getline(ss, token, delimiter))
    {
        pathTokens.push_back(token);
    }

    WindowRegisterInfo info;
    info.factory = []() { return std::unique_ptr<T>(new T()); };
    info.menuPath = pathTokens;
    GetSingleton().registery.push_back(std::move(info));
    return true;
}
} // namespace Editor
  //

#define DECLARE_EDITOR_WINDOW(TypeName)                                                                                \
    TypeName() {}                                                                                                      \
    static bool _editorWindowRegistered;                                                                               \
    friend class WindowRegistery;

#define DEFINE_EDITOR_WINDOW(TypeName, MenuPath)                                                                       \
    bool TypeName::_editorWindowRegistered = WindowRegistery::Register<TypeName>(MenuPath);
