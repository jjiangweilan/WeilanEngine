#ifdef WEILAN_ENABLE_MCP

#include "MCPServer.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>
#include <thread>
#include <memory>
#include <string>
#include <future>
#include <mutex>
#include <queue>

#ifdef _WIN32
#undef near
#undef far
#endif

#include "Engine/Runtime/System/SceneManager/SceneManager.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

namespace {

struct Message {
    json request;
    std::function<void(json)> onComplete;
};

struct SharedState {
    std::mutex queueMutex;
    std::queue<std::shared_ptr<Message>> messageQueue;
};

json MakeToolTextResult(const json& id, std::string text) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", {
                {
                    {"type", "text"},
                    {"text", std::move(text)}
                }
            }}
        }}
    };
}

json MakeToolErrorResult(const json& id, std::string text) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"isError", true},
            {"content", {
                {
                    {"type", "text"},
                    {"text", std::move(text)}
                }
            }}
        }}
    };
}

json AddPrimitiveAssetToActiveScene(const json& id, std::string_view path) {
    auto scene = SceneManager::GetActiveScene();
    if (scene == nullptr) {
        return MakeToolErrorResult(id, "No active scene is available");
    }

    auto model = dynamic_cast<Model*>(AssetDatabase::Singleton()->LoadAsset(path));
    if (model == nullptr || model->GetMeshes().empty() || model->GetMeshes()[0] == nullptr) {
        spdlog::error("Failed to create primitive from asset: {}", path);
        return MakeToolErrorResult(id, fmt::format("Failed to create primitive from asset: {}", path));
    }

    auto gameObject = std::make_unique<GameObject>();
    gameObject->SetName(model->GetName());
    gameObject->SetWantsToBeEnabled();

    auto meshRenderer = gameObject->AddComponent<MeshRenderer>();
    meshRenderer->SetMesh(model->GetMeshes()[0].get());
    meshRenderer->SetMaterial(EngineInternalResources::GetDefaultGridMaterial());
    gameObject->AddComponent<PhysicsBody>();

    gameObject->SetName("New GameObject");
    scene->AddGameObject(std::move(gameObject));

    return MakeToolTextResult(id, fmt::format("Added primitive asset to scene: {}", path));
}

json HandleJsonRpc(const json& request) {
    if (!request.is_object() || 
        !request.contains("jsonrpc") || 
        !request["jsonrpc"].is_string() || 
        request["jsonrpc"].get<std::string>() != "2.0") {
        return {
            {"jsonrpc", "2.0"},
            {"id", nullptr},
            {"error", {
                {"code", -32600},
                {"message", "Invalid Request"}
            }}
        };
    }

    std::string method = (request.contains("method") && request["method"].is_string()) ? 
                          request["method"].get<std::string>() : "";

    json id = nullptr;
    if (request.contains("id") && (request["id"].is_string() || request["id"].is_number() || request["id"].is_null())) {
        id = request["id"];
    } else {
        // Notification: the id field is absent. We do not send a response.
        return json();
    }

    if (method == "initialize") {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", {
                    {"tools", json::object()},
                    {"resources", json::object()}
                }},
                {"serverInfo", {
                    {"name", "WeilanEngineMCPServer"},
                    {"version", "1.0.0"}
                }}
            }}
        };
    } else if (method == "tools/list") {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"tools", {
                    {
                        {"name", "GetSceneGraph"},
                        {"description", "Gets the names of all root game objects in the active scene"},
                        {"inputSchema", {
                            {"type", "object"},
                            {"properties", json::object()}
                        }}
                    },
                    {
                        {"name", "AddPrimitiveAssetToScene"},
                        {"description", "Adds a primitive/model asset to the active scene"},
                        {"inputSchema", {
                            {"type", "object"},
                            {"properties", {
                                {"path", {
                                    {"type", "string"},
                                    {"description", "Asset path, for example _engine_internal/Models/Cube.fbx"}
                                }}
                            }},
                            {"required", {"path"}}
                        }}
                    }
                }}
            }}
        };
    } else if (method == "tools/call") {
        std::string toolName = (request.contains("params") && request["params"].contains("name") && request["params"]["name"].is_string()) 
                               ? request["params"]["name"].get<std::string>() : "";
        if (toolName == "GetSceneGraph") {
            json roots = json::array();
            auto scene = SceneManager::GetActiveScene();
            if (scene) {
                for (auto& root : scene->GetRootObjects()) {
                    if (root) roots.push_back(root->GetName());
                }
            }
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", {
                    {"content", {
                        {
                            {"type", "text"},
                            {"text", roots.dump()}
                        }
                    }}
                }}
            };
        } else if (toolName == "AddPrimitiveAssetToScene") {
            const json& arguments = request["params"].contains("arguments") ? request["params"]["arguments"] : json::object();
            if (!arguments.is_object() || !arguments.contains("path") || !arguments["path"].is_string()) {
                return MakeToolErrorResult(id, "AddPrimitiveAssetToScene requires a string 'path' argument");
            }

            return AddPrimitiveAssetToActiveScene(id, arguments["path"].get<std::string>());
        } else {
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"error", {
                    {"code", -32601},
                    {"message", "Tool not found"}
                }}
            };
        }
    } else if (method == "notifications/initialized") {
        // Notification, no response
        return json();
    } else if (method == "ping") {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", json::object()}
        };
    } else {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {
                {"code", -32601},
                {"message", "Method not found"}
            }}
        };
    }
}

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    net::steady_timer timer_;
    std::shared_ptr<SharedState> shared_state_;

public:
    explicit HttpConnection(tcp::socket socket, std::shared_ptr<SharedState> shared_state) 
        : socket_(std::move(socket)), 
          timer_(socket_.get_executor()),
          shared_state_(std::move(shared_state)) {}

    void Start() {
        ReadRequest();
    }

private:
    void ReadRequest() {
        http::async_read(socket_, buffer_, request_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) {
                    self->ProcessRequest();
                } else if (ec != http::error::end_of_stream && ec != net::error::operation_aborted) {
                    // Normal for connections to be closed by the client
                    spdlog::debug("MCP Server read error: {}", ec.message());
                }
            });
    }

    void ProcessRequest() {
        if (request_.method() == http::verb::options) {
            response_.version(request_.version());
            response_.result(http::status::ok);
            response_.set(http::field::server, "WeilanMCPServer");
            response_.set(http::field::access_control_allow_origin, "*");
            response_.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
            response_.set(http::field::access_control_allow_headers, "Content-Type");
            response_.keep_alive(request_.keep_alive());
            response_.prepare_payload();
            
            http::async_write(socket_, response_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    if (!ec && self->response_.keep_alive()) {
                        self->request_ = {};
                        self->response_ = {};
                        self->ReadRequest();
                    } else {
                        beast::error_code shutdown_ec;
                        self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                    }
                });
            return;
        }

        if (request_.method() == http::verb::get && request_.target() == "/sse") {
            auto res = std::make_shared<http::response<http::empty_body>>();
            res->version(request_.version());
            res->result(http::status::ok);
            res->set(http::field::server, "WeilanMCPServer");
            res->set(http::field::content_type, "text/event-stream");
            res->set(http::field::cache_control, "no-cache");
            res->set(http::field::connection, "keep-alive");
            res->set(http::field::access_control_allow_origin, "*");
            
            auto sr = std::make_shared<http::response_serializer<http::empty_body>>(*res);
            
            http::async_write_header(socket_, *sr,
                [self = shared_from_this(), res, sr](beast::error_code ec, std::size_t) {
                    if (!ec) {
                        // Immediately send the endpoint event
                        auto endpoint_msg = std::make_shared<std::string>("event: endpoint\ndata: http://localhost:8080/message\n\n");
                        net::async_write(self->socket_, net::buffer(*endpoint_msg),
                            [self, endpoint_msg](beast::error_code write_ec, std::size_t) {
                                if (!write_ec) {
                                    self->SendSSEPing();
                                    self->ReadUntilDisconnect();
                                }
                            });
                    }
                });
            return;
        }

        response_.version(request_.version());
        response_.keep_alive(request_.keep_alive());
        response_.set(http::field::server, "WeilanMCPServer");
        response_.set(http::field::access_control_allow_origin, "*");

        if (request_.method() == http::verb::post && request_.target() == "/message") {
            spdlog::info("MCP Server received message of size: {}", request_.body().size());
            
            try {
                json rpc_req = json::parse(request_.body());
                
                auto msg = std::make_shared<Message>();
                msg->request = std::move(rpc_req);
                
                msg->onComplete = [self = shared_from_this()](json rpc_res) {
                    net::post(self->socket_.get_executor(), [self, rpc_res]() {
                        if (rpc_res.is_null()) {
                            self->response_.result(http::status::no_content);
                            self->response_.set(http::field::content_type, "text/plain");
                            self->response_.body() = "";
                        } else {
                            self->response_.result(http::status::ok);
                            self->response_.set(http::field::content_type, "application/json");
                            self->response_.body() = rpc_res.dump();
                        }
                        self->response_.prepare_payload();

                        http::async_write(self->socket_, self->response_,
                            [self](beast::error_code ec, std::size_t) {
                                if (!ec && self->response_.keep_alive()) {
                                    self->request_ = {};
                                    self->response_ = {};
                                    self->ReadRequest();
                                } else {
                                    beast::error_code shutdown_ec;
                                    self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                                }
                            });
                    });
                };

                {
                    std::lock_guard<std::mutex> lock(shared_state_->queueMutex);
                    shared_state_->messageQueue.push(msg);
                }
                
                // Do not write anything yet; handled in onComplete callback
                return;
            } catch (const json::parse_error& e) {
                spdlog::error("JSON parse error: {}", e.what());
                json error_res = {
                    {"jsonrpc", "2.0"},
                    {"id", nullptr},
                    {"error", {
                        {"code", -32700},
                        {"message", "Parse error"}
                    }}
                };
                response_.result(http::status::ok);
                response_.set(http::field::content_type, "application/json");
                response_.body() = error_res.dump();
            } catch (const json::exception& e) {
                spdlog::error("JSON schema/type error: {}", e.what());
                json error_res = {
                    {"jsonrpc", "2.0"},
                    {"id", nullptr},
                    {"error", {
                        {"code", -32600},
                        {"message", "Invalid Request"}
                    }}
                };
                response_.result(http::status::bad_request);
                response_.set(http::field::content_type, "application/json");
                response_.body() = error_res.dump();
            }
        } else {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            response_.body() = "Not Found";
        }

        response_.prepare_payload();

        http::async_write(socket_, response_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec && self->response_.keep_alive()) {
                    self->request_ = {};
                    self->response_ = {};
                    self->ReadRequest();
                } else {
                    beast::error_code shutdown_ec;
                    self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                }
            });
    }

    void SendSSEPing() {
        timer_.expires_after(std::chrono::seconds(15));
        timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                auto ping = std::make_shared<std::string>(": keep-alive\n\n");
                net::async_write(self->socket_, net::buffer(*ping),
                    [self, ping](beast::error_code write_ec, std::size_t) {
                        if (!write_ec) {
                            self->SendSSEPing();
                        }
                    });
            }
        });
    }

    void ReadUntilDisconnect() {
        // Just read to detect client disconnects. We ignore the data.
        http::async_read(socket_, buffer_, request_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) {
                    self->timer_.cancel();
                    beast::error_code shutdown_ec;
                    self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                } else {
                    self->buffer_.consume(self->buffer_.size()); // Discard any body content sent by client on SSE connection
                    self->ReadUntilDisconnect();
                }
            });
    }
};

class HttpListener : public std::enable_shared_from_this<HttpListener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    net::steady_timer retry_timer_;
    std::shared_ptr<SharedState> shared_state_;

public:
    HttpListener(net::io_context& ioc, tcp::endpoint endpoint, std::shared_ptr<SharedState> shared_state)
        : ioc_(ioc), acceptor_(ioc), retry_timer_(ioc), shared_state_(std::move(shared_state)) {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { 
            spdlog::error("MCP Server open error: {}", ec.message()); 
            CloseAcceptor();
            return; 
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) { 
            spdlog::error("MCP Server reuse_address error: {}", ec.message()); 
            CloseAcceptor();
            return; 
        }

        acceptor_.bind(endpoint, ec);
        if (ec) { 
            spdlog::error("MCP Server bind error: {}", ec.message()); 
            CloseAcceptor();
            return; 
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { 
            spdlog::error("MCP Server listen error: {}", ec.message()); 
            CloseAcceptor();
            return; 
        }
    }

    void Start() {
        DoAccept();
    }

private:
    void CloseAcceptor() {
        beast::error_code close_ec;
        acceptor_.close(close_ec);
    }

    void DoAccept() {
        if (!acceptor_.is_open()) return;

        acceptor_.async_accept(
            net::make_strand(ioc_),
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
                if (ec == net::error::operation_aborted) return;
                if (!ec) {
                    std::make_shared<HttpConnection>(std::move(socket), self->shared_state_)->Start();
                    self->DoAccept();
                } else {
                    spdlog::error("MCP Server accept error: {}", ec.message());
                    // Delay slightly to prevent spin loops on resource exhaustion using async_wait
                    self->retry_timer_.expires_after(std::chrono::milliseconds(100));
                    self->retry_timer_.async_wait([self](beast::error_code) {
                        self->DoAccept();
                    });
                }
            });
    }
};

} // namespace

struct MCPServer::Impl {
    unsigned short port;
    boost::asio::io_context ioc;
    std::shared_ptr<HttpListener> listener;
    std::thread runner;
    std::shared_ptr<SharedState> shared_state;

    explicit Impl(unsigned short p) : port(p), shared_state(std::make_shared<SharedState>()) {}
};

MCPServer::MCPServer(unsigned short port)
    : pimpl(std::make_unique<Impl>(port)) {
}

MCPServer::~MCPServer() {
    Stop();
}

MCPServer::MCPServer(MCPServer&&) noexcept = default;
MCPServer& MCPServer::operator=(MCPServer&&) noexcept = default;

void MCPServer::Start() {
    spdlog::info("Starting MCP Server on port {}", pimpl->port);
    
    auto endpoint = tcp::endpoint(net::ip::make_address("127.0.0.1"), pimpl->port);
    pimpl->listener = std::make_shared<HttpListener>(pimpl->ioc, endpoint, pimpl->shared_state);
    pimpl->listener->Start();

    pimpl->runner = std::thread([this]() {
        pimpl->ioc.run();
    });
}

void MCPServer::Stop() {
    if (!pimpl) return;
    
    spdlog::info("Stopping MCP Server");
    pimpl->ioc.stop();

    if (pimpl->runner.joinable()) {
        pimpl->runner.join();
    }
}

void MCPServer::Tick() {
    if (!pimpl || !pimpl->shared_state) return;

    std::queue<std::shared_ptr<Message>> localQueue;
    {
        std::lock_guard<std::mutex> lock(pimpl->shared_state->queueMutex);
        std::swap(localQueue, pimpl->shared_state->messageQueue);
    }

    while (!localQueue.empty()) {
        auto msg = localQueue.front();
        localQueue.pop();

        try {
            json result = HandleJsonRpc(msg->request);
            if (msg->onComplete) msg->onComplete(result);
        } catch (...) {
            if (msg->onComplete) msg->onComplete(json());
        }
    }
}
# e n d i f  
 
