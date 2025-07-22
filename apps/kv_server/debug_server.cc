#include <seastar/core/app-template.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/http/httpd.hh>
#include <seastar/util/log.hh>
#include <iostream>

using namespace seastar;

static logger debug_logger("debug");

int main(int argc, char** argv) {
    app_template app;
    
    try {
        app.run(argc, argv, [] {
            debug_logger.info("Starting debug HTTP server");
            
            auto server = std::make_unique<seastar::httpd::http_server>("debug");
            
            // Simple test route
            server->_routes.put(seastar::httpd::operation_type::GET, "/test", 
                new seastar::httpd::function_handler([](seastar::httpd::const_req req) {
                    debug_logger.info("Test route hit!");
                    return R"({"message":"test route works"})";
                }, "json"));
            
            // KV test route
            server->_routes.add(seastar::httpd::operation_type::GET, 
                              seastar::httpd::url("/api/v1/kv/keys"),
                              new seastar::httpd::function_handler(
                                  [](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                                      debug_logger.info("KV list route hit!");
                                      auto reply = std::make_unique<seastar::http::reply>();
                                      reply->set_status(seastar::http::reply::status_type::ok);
                                      reply->write_body("json", R"({"message":"kv route works"})");
                                      return make_ready_future<std::unique_ptr<seastar::http::reply>>(std::move(reply));
                                  }, "json"));
            
            return server->listen(socket_address(ipv4_addr("127.0.0.1", 8081))).then([&server] {
                debug_logger.info("Debug server listening on port 8081");
                debug_logger.info("Test with: curl http://localhost:8081/test");
                debug_logger.info("Test with: curl http://localhost:8081/api/v1/kv/keys");
                
                return seastar::keep_doing([] {
                    return seastar::sleep(std::chrono::seconds(1));
                });
            });
        });
    } catch (...) {
        std::cerr << "Failed to start debug server: " << std::current_exception() << std::endl;
        return 1;
    }
    
    return 0;
}