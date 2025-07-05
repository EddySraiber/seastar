#include "kv_store.hh"
#include <seastar/core/coroutine.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/util/defer.hh>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <seastar/util/short_streams.hh>

using namespace seastar;

// Utility functions implementation
sstring json_escape(const sstring& str) {
    sstring escaped;
    for (char c : str) {
        switch (c) {
            case '"': escaped += sstring("\\\""); break;
            case '\\': escaped += sstring("\\\\"); break;
            case '\b': escaped += sstring("\\b"); break;
            case '\f': escaped += sstring("\\f"); break;
            case '\n': escaped += sstring("\\n"); break;
            case '\r': escaped += sstring("\\r"); break;
            case '\t': escaped += sstring("\\t"); break;
            default:
                if (c < 0x20) {
                    char hex[7];
                    snprintf(hex, sizeof(hex), "\\u%04x", (unsigned char)c);
                    escaped += sstring(hex);
                } else {
                    escaped += sstring(1, c);
                }
        }
    }
    return escaped;
}
sstring url_decode(const sstring& encoded) {
    sstring decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            auto hex = encoded.substr(i + 1, 2);
            char decoded_char = static_cast<char>(std::stoi(hex.c_str(), nullptr, 16));
            decoded += sstring(1, decoded_char);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded += sstring(" ");
        } else {
            decoded += sstring(1, encoded[i]);
        }
    }
    return decoded;
}

sstring url_encode(const sstring& decoded) {
    sstring encoded;
    for (char c : decoded) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += sstring(1, c);
        } else {
            encoded += sstring("%");
            char hex[3];
            snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned char>(c));
            encoded += sstring(hex);
        }
    }
    return encoded;
}


// Persistent KV Store implementation
persistent_kv_store::persistent_kv_store(const sstring& data_dir, size_t cache_size)
    : _data_dir(data_dir), _cache(cache_size) {
    _log_file_path = _data_dir + "/kv_log_" + std::to_string(this_shard_id()) + ".log";
}

future<> persistent_kv_store::start() {
    return recursive_touch_directory(_data_dir).then([this] {
        return _load_from_disk();
    }).then([this] {
        return _ensure_log_file_open();
    });
}

future<> persistent_kv_store::stop() {
    return _log_stream.close().then([this] {
        return _log_file.close();
    }).handle_exception([](std::exception_ptr ep) {
        // Log the exception but don't fail the shutdown
        std::cerr << "Error during shutdown: " << ep << std::endl;
    });
}

future<> persistent_kv_store::_ensure_log_file_open() {
    return open_file_dma(_log_file_path, open_flags::create | open_flags::wo).then([this](file f) {
        _log_file = std::move(f);
        return _log_file.size().then([this](size_t size) {
            return make_file_output_stream(_log_file, size);
        }).then([this](output_stream<char> stream) {
            _log_stream = std::move(stream);
        });
    });
}

future<> persistent_kv_store::_write_log_entry(const sstring& operation, const sstring& key, const sstring& value) {
    // Format: timestamp|operation|key_length|key|value_length|value\n
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << now << "|" << operation << "|" << key.length() << "|" << key << "|" << value.length() << "|" << value << "\n";
    
    sstring log_entry = oss.str();
    
    return _log_stream.write(log_entry.c_str(), log_entry.length()).then([this] {
        return _log_stream.flush();
    });
}

future<> persistent_kv_store::_load_from_disk() {
    return open_file_dma(_log_file_path, open_flags::ro).then([this](file f) {
        return f.size().then([this, f = std::move(f)](size_t size) mutable {
            if (size == 0) {
                return f.close().then([this] {
                    return make_ready_future<>();
                });
            }
            
            auto stream = make_file_input_stream(std::move(f));
            return seastar::util::read_entire_stream_contiguous(stream).then([this](sstring content) {
                // Parse log entries
                std::string std_content(content.c_str());
                std::istringstream iss(std_content);
                std::string std_line;
                
                while (std::getline(iss, std_line)) {
                    sstring line(std_line.c_str());
                    if (line.empty()) continue;
                    
                    // Parse: timestamp|operation|key_length|key|value_length|value
                    // Split by | but handle key and value separately due to length prefixes
                    auto first_pipe = line.find('|');
                    auto second_pipe = line.find('|', first_pipe + 1);
                    auto third_pipe = line.find('|', second_pipe + 1);
                    
                    if (first_pipe == sstring::npos || second_pipe == sstring::npos || third_pipe == sstring::npos) {
                        continue; // Skip malformed lines
                    }
                    
                    sstring timestamp = line.substr(0, first_pipe);
                    sstring operation = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
                    sstring key_len_str = line.substr(second_pipe + 1, third_pipe - second_pipe - 1);
                    
                    size_t key_len = std::stoul(key_len_str.c_str());
                    sstring key = line.substr(third_pipe + 1, key_len);
                    
                    auto fourth_pipe = line.find('|', third_pipe + 1 + key_len);
                    if (fourth_pipe == sstring::npos) continue;
                    
                    sstring value_len_str = line.substr(fourth_pipe + 1);
                    auto fifth_pipe = line.find('|', fourth_pipe + 1);
                    if (fifth_pipe != sstring::npos) {
                        value_len_str = line.substr(fourth_pipe + 1, fifth_pipe - fourth_pipe - 1);
                    }
                    
                    size_t value_len = std::stoul(value_len_str.c_str());
                    sstring value = line.substr(fifth_pipe + 1, value_len);
                    
                    // Apply operation
                    if (operation == "PUT") {
                        _persistent_data[key] = value;
                    } else if (operation == "DELETE") {
                        _persistent_data.erase(key);
                    }
                }
            });
        });
    }).handle_exception([](std::exception_ptr ep) {
        // File might not exist, that's OK
        return make_ready_future<>();
    });
}

future<std::optional<sstring>> persistent_kv_store::get(const sstring& key) {
    if (key.length() > 255) {
        return make_ready_future<std::optional<sstring>>(std::nullopt);
    }
    
    // Check cache first
    auto cached_value = _cache.get(key);
    if (cached_value) {
        return make_ready_future<std::optional<sstring>>(*cached_value);
    }
    
    // Check persistent storage
    auto it = _persistent_data.find(key);
    if (it != _persistent_data.end()) {
        // Add to cache
        _cache.put(key, it->second);
        return make_ready_future<std::optional<sstring>>(it->second);
    }
    
    return make_ready_future<std::optional<sstring>>(std::nullopt);
}

future<> persistent_kv_store::put(const sstring& key, const sstring& value) {
    if (key.length() > 255) {
        return make_exception_future<>(std::invalid_argument("Key too long"));
    }
    
    // Update cache
    _cache.put(key, value);
    
    // Update persistent storage
    _persistent_data[key] = value;
    
    // Write to log
    return _write_log_entry("PUT", key, value);
}

future<> persistent_kv_store::remove(const sstring& key) {
    if (key.length() > 255) {
        return make_exception_future<>(std::invalid_argument("Key too long"));
    }
    
    // Remove from cache
    _cache.remove(key);
    
    // Remove from persistent storage
    _persistent_data.erase(key);
    
    // Write to log
    return _write_log_entry("DELETE", key);
}

future<std::vector<sstring>> persistent_kv_store::get_all_keys() {
    std::vector<sstring> keys;
    
    // Get all keys from persistent storage
    for (const auto& pair : _persistent_data) {
        keys.push_back(pair.first);
    }
    
    // Sort keys
    std::sort(keys.begin(), keys.end());
    
    return make_ready_future<std::vector<sstring>>(std::move(keys));
}

// HTTP API handlers implementation
future<std::unique_ptr<seastar::http::reply>> kv_api_handler::handle_get(std::unique_ptr<seastar::http::request> req) {
    sstring key = url_decode(req->param["key"]);
    
    if (key.length() > 255) {
        auto reply = std::make_unique<seastar::http::reply>();
        reply->set_status(seastar::http::reply::status_type::bad_request);
        reply->write_body("json", "{\"status\":\"error\",\"message\":\"Key too long (max 255 bytes)\"}");
        return make_ready_future<std::unique_ptr<seastar::http::reply>>(std::move(reply));
    }
    
    uint32_t shard_id = std::hash<sstring>{}(key) % smp::count;
    
    return _stores.invoke_on(shard_id, [key](persistent_kv_store& store) {
        return store.get(key);
    }).then([](std::optional<sstring> value) {
        auto reply = std::make_unique<seastar::http::reply>();
        reply->set_status(seastar::http::reply::status_type::ok);
        
        if (value) {
            sstring json_body = R"({"status":"success","data":{"value":")" + json_escape(*value) + R"("}})";
            reply->write_body("json", json_body);
        } else {
            reply->set_status(seastar::http::reply::status_type::not_found);
            reply->write_body("json", R"({"status":"error","message":"Key not found"})");
        }
        
        return reply;
    }).handle_exception([](std::exception_ptr ep) {
        auto reply = std::make_unique<seastar::http::reply>();
        reply->set_status(seastar::http::reply::status_type::internal_server_error);
        reply->write_body("json", "{\"status\":\"error\",\"message\":\"Internal server error\"}");
        return reply;
    });
}

future<std::unique_ptr<http::reply>> kv_api_handler::handle_put(std::unique_ptr<http::request> req) {
    sstring key = url_decode(req->param["key"]);
    sstring value = req->content;
    
    if (key.length() > 255) {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::bad_request);
        reply->write_body("json", "{\"status\":\"error\",\"message\":\"Key too long (max 255 bytes)\"}");
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(reply));
    }
    
    uint32_t shard_id = std::hash<sstring>{}(key) % smp::count;
    
    return _stores.invoke_on(shard_id, [key, value](persistent_kv_store& store) {
        return store.put(key, value);
    }).then([]() {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::ok);
        reply->write_body("json", R"({"status":"success","message":"Key stored successfully"})");
        return reply;
    }).handle_exception([](std::exception_ptr ep) {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::bad_request);
        reply->write_body("json", R"({"status":"error","message":"Failed to store key"})");
        return reply;
    });
}

future<std::unique_ptr<http::reply>> kv_api_handler::handle_delete(std::unique_ptr<http::request> req) {
    sstring key = url_decode(req->param["key"]);
    
    if (key.length() > 255) {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::bad_request);
        reply->write_body("json", "{\"status\":\"error\",\"message\":\"Key too long (max 255 bytes)\"}");
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(reply));
    }
    
    uint32_t shard_id = std::hash<sstring>{}(key) % smp::count;
    
    return _stores.invoke_on(shard_id, [key](persistent_kv_store& store) {
        return store.remove(key);
    }).then([]() {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::ok);
        reply->write_body("json", R"({"status":"success","message":"Key deleted successfully"})");
        return reply;
    }).handle_exception([](std::exception_ptr ep) {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::internal_server_error);
        reply->write_body("json", R"({"status":"error","message":"Failed to delete key"})");
        return reply;
    });
}

future<std::unique_ptr<http::reply>> kv_api_handler::handle_list_keys(std::unique_ptr<http::request> req) {
    // Collect keys from all shards
    return _stores.map_reduce0([](persistent_kv_store& store) {
        return store.get_all_keys();
    }, std::vector<sstring>{}, [](std::vector<sstring> acc, std::vector<sstring> shard_keys) {
        acc.insert(acc.end(), shard_keys.begin(), shard_keys.end());
        return acc;
    }).then([](std::vector<sstring> all_keys) {
        // Sort all keys
        std::sort(all_keys.begin(), all_keys.end());
        
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::ok);
        
        std::ostringstream oss;
        oss << R"({"status":"success","data":{"keys":[)";
        for (size_t i = 0; i < all_keys.size(); ++i) {
            if (i > 0) oss << ",";
            oss << '"' << json_escape(all_keys[i]) << '"';
        }
        oss << "]}}";
        reply->write_body("json", oss.str());
        return reply;
    }).handle_exception([](std::exception_ptr ep) {
        auto reply = std::make_unique<http::reply>();
        reply->set_status(http::reply::status_type::internal_server_error);
        reply->write_body("json", R"({"status":"error","message":"Failed to list keys"})");
        return reply;
    });
}

void kv_api_handler::setup_routes(seastar::httpd::http_server& server) {
    server._routes.add(seastar::httpd::operation_type::GET, 
                      seastar::httpd::url("/api/v1/kv/keys/{key}"),
                      new seastar::httpd::function_handler(
                          [this](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                              return this->handle_get(std::move(req));
                          }, "json"));
    
    server._routes.add(seastar::httpd::operation_type::PUT,
                      seastar::httpd::url("/api/v1/kv/keys/{key}"),
                      new seastar::httpd::function_handler(
                          [this](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                              return this->handle_put(std::move(req));
                          }, "json"));
    
    server._routes.add(seastar::httpd::operation_type::DELETE,
                      seastar::httpd::url("/api/v1/kv/keys/{key}"),
                      new seastar::httpd::function_handler(
                          [this](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                              return this->handle_delete(std::move(req));
                          }, "json"));
    
    server._routes.add(seastar::httpd::operation_type::GET,
                      seastar::httpd::url("/api/v1/kv/keys"),
                      new seastar::httpd::function_handler(
                          [this](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                              return this->handle_list_keys(std::move(req));
                          }, "json"));
}