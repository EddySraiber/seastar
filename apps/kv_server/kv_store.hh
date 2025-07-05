#pragma once

#include <seastar/core/seastar.hh>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>
#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/sstring.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/core/when_all.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/sleep.hh>
#include <seastar/util/log.hh>
#include <seastar/json/json_elements.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/http/httpd.hh>
#include <seastar/http/handlers.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/http/file_handler.hh>
#include <seastar/http/api_docs.hh>
#include <seastar/http/request.hh>
#include <seastar/http/reply.hh>

#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace seastar;

// LRU Cache implementation
template<typename K, typename V>
class lru_cache {
private:
    struct cache_entry {
        K key;
        V value;
        std::chrono::steady_clock::time_point last_accessed;
        
        cache_entry(K k, V v) : key(std::move(k)), value(std::move(v)), 
                               last_accessed(std::chrono::steady_clock::now()) {}
    };
    
    using cache_iterator = typename std::list<cache_entry>::iterator;
    
    std::list<cache_entry> _cache_list;
    std::unordered_map<K, cache_iterator> _cache_map;
    size_t _max_size;
    
public:
    explicit lru_cache(size_t max_size) : _max_size(max_size) {}
    
    std::optional<V> get(const K& key) {
        auto it = _cache_map.find(key);
        if (it != _cache_map.end()) {
            // Move to front (most recently used)
            it->second->last_accessed = std::chrono::steady_clock::now();
            _cache_list.splice(_cache_list.begin(), _cache_list, it->second);
            return it->second->value;
        }
        return std::nullopt;
    }
    
    void put(const K& key, const V& value) {
        auto it = _cache_map.find(key);
        if (it != _cache_map.end()) {
            // Update existing entry
            it->second->value = value;
            it->second->last_accessed = std::chrono::steady_clock::now();
            _cache_list.splice(_cache_list.begin(), _cache_list, it->second);
        } else {
            // Add new entry
            _cache_list.emplace_front(key, value);
            _cache_map[key] = _cache_list.begin();
            
            // Evict if necessary
            if (_cache_list.size() > _max_size) {
                auto last = _cache_list.end();
                --last;
                _cache_map.erase(last->key);
                _cache_list.pop_back();
            }
        }
    }
    
    void remove(const K& key) {
        auto it = _cache_map.find(key);
        if (it != _cache_map.end()) {
            _cache_list.erase(it->second);
            _cache_map.erase(it);
        }
    }
    
    size_t size() const {
        return _cache_list.size();
    }
    
    std::vector<K> get_all_keys() const {
        std::vector<K> keys;
        for (const auto& entry : _cache_list) {
            keys.push_back(entry.key);
        }
        return keys;
    }
};

// Persistent key-value store implementation
class persistent_kv_store {
private:
    sstring _data_dir;
    sstring _log_file_path;
    file _log_file;
    output_stream<char> _log_stream;
    
    lru_cache<sstring, sstring> _cache;
    std::unordered_map<sstring, sstring> _persistent_data;
    
    // Sharding key function
    uint32_t _shard_for_key(const sstring& key) const {
        std::hash<sstring> hasher;
        return hasher(key) % smp::count;
    }
    
    // Persistence operations
    future<> _write_log_entry(const sstring& operation, const sstring& key, const sstring& value = "");
    future<> _load_from_disk();
    future<> _ensure_log_file_open();
    
public:
    explicit persistent_kv_store(const sstring& data_dir, size_t cache_size = 1000);
    
    future<> start();
    future<> stop();
    
    // Core operations
    future<std::optional<sstring>> get(const sstring& key);
    future<> put(const sstring& key, const sstring& value);
    future<> remove(const sstring& key);
    future<std::vector<sstring>> get_all_keys();
    
    // Statistics
    size_t cache_size() const { return _cache.size(); }
    size_t persistent_size() const { return _persistent_data.size(); }
};

// HTTP API handlers
class kv_api_handler {
private:
    distributed<persistent_kv_store>& _stores;
    
public:
    explicit kv_api_handler(distributed<persistent_kv_store>& stores) : _stores(stores) {}
    
    // REST API endpoints
    future<std::unique_ptr<seastar::http::reply>> handle_get(std::unique_ptr<seastar::http::request> req);
    future<std::unique_ptr<seastar::http::reply>> handle_put(std::unique_ptr<seastar::http::request> req);
    future<std::unique_ptr<seastar::http::reply>> handle_delete(std::unique_ptr<seastar::http::request> req);
    future<std::unique_ptr<seastar::http::reply>> handle_list_keys(std::unique_ptr<seastar::http::request> req);
    
    
    void setup_routes(seastar::httpd::http_server& server);
};

// Configuration
struct kv_config {
    uint16_t port = 8080;
    sstring data_dir = "/tmp/kv_store";
    size_t cache_size = 1000;
    sstring bind_address = "127.0.0.1";
};

// Utility functions
sstring url_decode(const sstring& encoded);
sstring url_encode(const sstring& decoded);
sstring json_escape(const sstring& str);