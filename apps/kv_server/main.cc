#include "kv_store.hh"
#include <seastar/core/app-template.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/distributed.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/sleep.hh>
#include <seastar/http/httpd.hh>
#include <seastar/util/log.hh>
#include <seastar/util/defer.hh>
#include <boost/program_options.hpp>
#include <iostream>
#include <sstream>

namespace bpo = boost::program_options;

using namespace seastar;

static logger kv_logger("kv_store");

class kv_server {
private:
    distributed<persistent_kv_store> _stores;
    std::unique_ptr<seastar::httpd::http_server> _server;
    std::unique_ptr<kv_api_handler> _api_handler;
    kv_config _config;

public:
    explicit kv_server(kv_config config) : _config(std::move(config)) {}

    future<> start() {
        kv_logger.info("Starting KV server on {}:{}", _config.bind_address, _config.port);
        
        return _stores.start(_config.data_dir, _config.cache_size).then([this] {
            return _stores.invoke_on_all(&persistent_kv_store::start);
        }).then([this] {
            _server = std::make_unique<seastar::httpd::http_server>("kv_server");
            _api_handler = std::make_unique<kv_api_handler>(_stores);
            
            _api_handler->setup_routes(*_server);
            
            // Setup health endpoint
            _server->_routes.put(seastar::httpd::operation_type::GET, "/health", 
                new seastar::httpd::function_handler([](seastar::httpd::const_req req) {
                    return R"({"status":"success","message":"Server is healthy"})";
                }, "json"));
            
            // Setup basic stats endpoint (simplified for now)
            _server->_routes.put(seastar::httpd::operation_type::GET, "/stats", 
                new seastar::httpd::function_handler([](seastar::httpd::const_req req) {
                    std::ostringstream oss;
                    oss << R"({"status":"success","data":{"stats":{"shard_count":)" << smp::count << R"(}}})";
                    return oss.str();
                }, "json"));
            
            return _server->listen(socket_address(ipv4_addr(std::string(_config.bind_address), _config.port)));
        }).then([this] {
            kv_logger.info("KV server started successfully");
            kv_logger.info("Available endpoints:");
            kv_logger.info("  GET    /api/v1/kv/keys/{{key}}     - Get value for key");
            kv_logger.info("  PUT    /api/v1/kv/keys/{{key}}     - Set value for key");
            kv_logger.info("  DELETE /api/v1/kv/keys/{{key}}     - Delete key");
            kv_logger.info("  GET    /api/v1/kv/keys            - List all keys");
            kv_logger.info("  GET    /health                    - Health check");
            kv_logger.info("  GET    /stats                     - Server statistics");
        });
    }

    future<> stop() {
        kv_logger.info("Stopping KV server");
        
        auto stop_server = _server ? _server->stop() : make_ready_future<>();
        
        return stop_server.then([this] {
            return _stores.invoke_on_all(&persistent_kv_store::stop);
        }).then([this] {
            return _stores.stop();
        }).then([this] {
            kv_logger.info("KV server stopped");
        });
    }

private:
};

// This function was causing duplicate option definitions - removed

future<> run_server(kv_config config) {
    return seastar::async([config = std::move(config)] {
        auto server = std::make_unique<kv_server>(std::move(config));
        server->start().get();
        
        kv_logger.info("Server is running. Press Ctrl+C to stop.");
        
        // Simple infinite loop - will be terminated by signal
        try {
            while (true) {
                seastar::sleep(std::chrono::seconds(1)).get();
            }
        } catch (...) {
            // Interrupted or exception occurred
        }
        
        kv_logger.info("Stopping server...");
        server->stop().get();
        kv_logger.info("Server stopped.");
    });
}

int main(int argc, char** argv) {
    app_template app;
    
    kv_config config;
    
    app.add_options()
        ("port", bpo::value<uint16_t>()->default_value(8080), "HTTP server port")
        ("bind-address", bpo::value<sstring>()->default_value("127.0.0.1"), "HTTP server bind address")
        ("data-dir", bpo::value<sstring>()->default_value("/tmp/kv_store"), "Data directory for persistence")
        ("cache-size", bpo::value<size_t>()->default_value(1000), "LRU cache size per shard");

    try {
        app.run(argc, argv, [&] {
            auto& args = app.configuration();
            config.port = args["port"].as<uint16_t>();
            config.bind_address = args["bind-address"].as<sstring>();
            config.data_dir = args["data-dir"].as<sstring>();
            config.cache_size = args["cache-size"].as<size_t>();
            
            std::cout << "Starting KV server with config:" << std::endl;
            std::cout << "  Port: " << config.port << std::endl;
            std::cout << "  Bind address: " << config.bind_address << std::endl;
            std::cout << "  Data directory: " << config.data_dir << std::endl;
            std::cout << "  Cache size per shard: " << config.cache_size << std::endl;
            
            return run_server(std::move(config));
        });
    } catch (...) {
        std::cerr << "Failed to start server: " << std::current_exception() << std::endl;
        return 1;
    }
    
    return 0;
}