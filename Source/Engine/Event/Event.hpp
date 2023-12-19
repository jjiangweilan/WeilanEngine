#pragma once

struct WindowSizeChange
{
    bool state;
    int width;
    int height;
};

struct WindowClose
{
    bool state;
};

class Event
{
public:
    void Poll();

    const WindowSizeChange& GetWindowSizeChanged()
    {
        return windowSizeChange;
    }
    const WindowClose& GetWindowClose()
    {
        return windowClose;
    }

private:
    WindowSizeChange windowSizeChange;
    WindowClose windowClose;

    void Reset();
};
