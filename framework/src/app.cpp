#include <blaze/app.h>
#include <blaze/exceptions.h>
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>
#include "server.h"

namespace blaze {

App::App() = default;

App::~App() {
    if(!ioc_.stopped()) {
        ioc_.stop();
    }
}

void App::ws(const std::string& path, WebSocketHandlers handlers) {
    ws_routes_[path] = std::move(handlers);
}

boost::asio::awaitable<void> delay(std::chrono::milliseconds ms) {
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer(executor, ms);
    co_await timer.async_wait(boost::asio::use_awaitable);
}

void App::spawn(Async<void> task) {
    boost::asio::co_spawn(ioc_.get_executor(), std::move(task), boost::asio::detached);
}

void App::_register_ws(const std::string& path, const std::shared_ptr<WebSocket>& ws) {
    std::lock_guard<std::mutex> lock(ws_mtx_);
    ws_sessions_[path].push_back(ws);
}

void App::broadcast_raw(const std::string& path, const std::string& payload) {
    std::lock_guard<std::mutex> lock(ws_mtx_);
    auto it = ws_sessions_.find(path);
    if (it == ws_sessions_.end()) {
        return;
    }

    auto& sessions = it->second;
    for (auto sit = sessions.begin(); sit != sessions.end(); ) {
        if (auto ws = sit->lock()) {
            try {
                ws->send(payload);
                ++sit;
            } catch (...) {
                sit = sessions.erase(sit);
            }
        } else {
            sit = sessions.erase(sit);
        }
    }
}

const WebSocketHandlers* App::get_ws_handler(const std::string& path) const {
    auto it = ws_routes_.find(path);
    if (it != ws_routes_.end()) {
        return &it->second;
    }
    return nullptr;
}

Router &App::get_router() {
    return router_;
}

boost::asio::awaitable<void> App::run_middleware(size_t index, Request& req, Response& res, const Handler& final_handler) {
    if (index < middleware_.size()) {
        const auto& mw = middleware_[index];
        co_await mw(req, res, [this, index, &req, &res, &final_handler]() -> boost::asio::awaitable<void> {
            co_await run_middleware(index + 1, req, res, final_handler);
        });
    } else {
        co_await final_handler(req, res);
    }
}

boost::asio::awaitable<Response> App::handle_request(Request& req, const std::string& client_ip, const bool keep_alive) {
    const auto start_time = std::chrono::steady_clock::now();
    Response res;
    int status_code = 500;

    try {
        req.set("client_ip", client_ip);
        req._set_services(&services_);
        const auto match = router_.match(req.method, req.path);
        
        Handler handler;
        if (match.has_value()) {
            req.params = match->params;
            req.path_values = match->path_values;
            handler = match->handler;
        } else {
            handler = [](Request&, Response& res) -> boost::asio::awaitable<void> {
                res.status(404).send("404 Not Found\n");
                co_return;
            };
        }

        // Run the chain
        co_await run_middleware(0, req, res, handler);

        status_code = res.get_status();

    } catch (const HttpError& e) {
        res.status(e.status()).json({
            {"error", "HTTP Error"},
            {"message", e.what()}
        });
        status_code = e.status();
    } catch (const std::exception& e) {
        res.status(500).json({
            {"error", "Internal Server Error"},
            {"message", e.what()}
        });
        status_code = 500;
        Logger::instance().log_error(std::string("Exception in handle_request: ") + e.what());
    }

    if (keep_alive) {
        res.header("Connection", "keep-alive");
    } else {
        res.header("Connection", "close");
    }

    res.header("Server", config_.server_name);

    // Async Logger
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    Logger::instance().log_access(client_ip, req.method, req.path, status_code, duration);

    co_return res;
}

void App::_register_docs() {
    // Register Documentation Routes
    this->get("/openapi.json", [this]() -> Async<Json> {
        boost::json::object spec;
        spec["openapi"] = "3.0.0";
        spec["info"] = {{"title", "Blaze API"}, {"version", "1.0.0"}};
        
        boost::json::object paths;
        for (const auto& doc : router_.docs()) {
            boost::json::object op;
            op["summary"] = doc.summary;
            
            // Path Params
            if (!doc.path_params.empty()) {
                boost::json::array params;
                for (const auto& p : doc.path_params) {
                    params.push_back({{"name", p.first}, {"in", "path"}, {"required", true}, {"schema", p.second}});
                }
                op["parameters"] = params;
            }

            // Body
            if (!doc.request_body.empty()) {
                op["requestBody"] = {{"content", {{"application/json", {{"schema", doc.request_body}}}}}};
            }

            op["responses"] = {{"200", {{"description", "OK"}, {"content", {{"application/json", {{"schema", doc.response_schema}}}}}}}};
            
            std::string method_low = doc.method;
            std::transform(method_low.begin(), method_low.end(), method_low.begin(), ::tolower);
            
            if (!paths.contains(doc.path)) paths[doc.path] = boost::json::object{};
            paths[doc.path].as_object()[method_low] = op;
        }
        spec["paths"] = paths;
        co_return Json(spec);
    });

    this->get("/docs", [](Response& res) -> Async<void> {
        res.header("Content-Type", "text/html");
        res.send(R"(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="utf-8" />
                <meta name="viewport" content="width=device-width, initial-scale=1" />
                <title>Blaze API Docs</title>
                <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui.css" />
            </head>
            <body>
                <div id="swagger-ui"></div>
                <script src="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui-bundle.js" crossorigin></script>
                <script>
                    window.onload = () => {
                        window.ui = SwaggerUIBundle({
                            url: '/openapi.json',
                            dom_id: '#swagger-ui',
                        });
                    };
                </script>
            </body>
            </html>
        )");
        co_return;
    });
}

void App::stop() {
    if (stopping_.exchange(true)) {
        return; // Already stopping
    }

    {
        std::lock_guard lock(lifecycle_mtx_);
        // Cancel signal waiting
        if (signals_) {
            signals_->cancel();
        }

        // Close listeners
        for (auto& listener : listeners_) {
            listener->stop();
        }
    }
    
    // Close WebSockets
    {
        std::lock_guard<std::mutex> lock(ws_mtx_);
        for (auto& [path, sessions] : ws_sessions_) {
            for (auto& weak_ws : sessions) {
                if (auto ws = weak_ws.lock()) {
                    ws->close();
                }
            }
        }
    }

    // Safety timeout
    int timeout = config_.shutdown_timeout;
    if (timeout > 0) {
        std::thread([this, timeout]() {
            std::this_thread::sleep_for(std::chrono::seconds(timeout));
            if (!ioc_.stopped()) {
                std::cerr << "[Blaze] Shutdown timeout reached. Force stopping..." << std::endl;
                ioc_.stop();
            }
        }).detach();
    } else {
        ioc_.stop();
    }
}

void App::listen(const int port, int num_threads) {
    Logger::instance().configure(config_.log_path);
    Logger::instance().set_level(config_.log_level);
    
    if (config_.enable_docs) {
        _register_docs();
    }

    if (num_threads <= 0) {
        num_threads = config_.num_threads;
    }

    if (num_threads <= 0) {
        num_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (num_threads == 0) num_threads = 4;
    }

    auto const address = net::ip::make_address("0.0.0.0");
    auto const endpoint = net::ip::tcp::endpoint{address, static_cast<unsigned short>(port)};

    try {
        // Create and launch listening port
        auto listener = std::make_shared<Listener>(ioc_, endpoint, *this);
        {
            std::lock_guard<std::mutex> lock(lifecycle_mtx_);
            listeners_.push_back(listener);
        }
        listener->run();
    } catch (const std::exception& e) {
        std::cerr << "[Blaze] FATAL: Could not start listener: " << e.what() << std::endl;
        throw; // Crash the process so blaze dev knows we failed
    }

    // (Ctrl+C) to stop cleanly
    {
        std::lock_guard<std::mutex> lock(lifecycle_mtx_);
        signals_ = std::make_unique<net::signal_set>(ioc_, SIGINT, SIGTERM);
        signals_->async_wait([this](boost::system::error_code const& ec, int signal_number) {
            if (ec == net::error::operation_aborted) return;
            if (ec) {
                std::cerr << "Signal error: " << ec.message() << std::endl;
                return;
            }
            std::cout << "[Blaze] Received signal " << signal_number << ", stopping..." << std::endl;
            this->stop();
        });
    }

    _run_server(num_threads);
}

void App::listen_ssl(const int port, const std::string& cert_path, const std::string& key_path, int num_threads) {
    Logger::instance().configure(config_.log_path);
    Logger::instance().set_level(config_.log_level);

    if (config_.enable_docs) {
        _register_docs();
    }

    if (num_threads <= 0) {
        num_threads = config_.num_threads;
    }

    if (num_threads <= 0) {
        num_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (num_threads == 0) num_threads = 4; // Fallback
    }

    try {
        ssl_ctx_.use_certificate_chain_file(cert_path);
        ssl_ctx_.use_private_key_file(key_path, ssl::context::pem);
    } catch (const std::exception& e) {
        std::cerr << "[App] SSL Error: " << e.what() << "\n";
        return;
    }

    auto const address = net::ip::make_address("0.0.0.0");
    auto const endpoint = net::ip::tcp::endpoint{address, static_cast<unsigned short>(port)};

    // Create and launch SSL listening port
    auto listener = std::make_shared<SslListener>(ioc_, ssl_ctx_, endpoint, *this);
    {
        std::lock_guard<std::mutex> lock(lifecycle_mtx_);
        listeners_.push_back(listener);
    }
    listener->run();

    // (Ctrl+C) to stop cleanly
    {
        std::lock_guard<std::mutex> lock(lifecycle_mtx_);
        signals_ = std::make_unique<net::signal_set>(ioc_, SIGINT, SIGTERM);
        signals_->async_wait([this](boost::system::error_code const& ec, int signal_number) {
            if (ec == net::error::operation_aborted) return;
            if (ec) {
                std::cerr << "Signal error: " << ec.message() << std::endl;
                return;
            }
            std::cout << "[Blaze] Received signal " << signal_number << ", stopping..." << std::endl;
            this->stop();
        });
    }

    std::cout << "[App] Starting HTTPS on port " << port << " with " << num_threads << " threads\n";

    _run_server(num_threads);
}

void App::_run_server(int num_threads) {
    // Run the IO Context on n threads
    std::vector<std::thread> v;
    v.reserve(num_threads - 1);
    for(auto i = num_threads - 1; i > 0; --i)
        v.emplace_back([this]{
            ioc_.run();
        });
    
    // Run on the main thread too
    ioc_.run();
    
    for(auto& t : v)
        if(t.joinable()) t.join();
}

void App::use(const Middleware &mw) {
    middleware_.push_back(mw);
}

RouteGroup App::group(const std::string& prefix) {
    return RouteGroup(router_, prefix);
}

} // namespace blaze
