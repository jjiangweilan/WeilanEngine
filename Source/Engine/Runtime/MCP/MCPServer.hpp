#pragma once

#include <memory>

class MCPServer {
public:
    explicit MCPServer(unsigned short port);
    ~MCPServer();

    MCPServer(const MCPServer&) = delete;
    MCPServer& operator=(const MCPServer&) = delete;
    MCPServer(MCPServer&&) noexcept;
    MCPServer& operator=(MCPServer&&) noexcept;

    void Start();
    void Stop();
    void Tick();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};
