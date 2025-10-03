#pragma once
#include <functional>

class FileWatcher
{
public:
    void RegisterOnFileAdded(std::function<void(const char* path)>&& f);
};
