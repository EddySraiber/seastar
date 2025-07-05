#include "kv_store.hh"
#include <iostream>
#include <cassert>
#include <string>

using namespace std;

void test_lru_basic() {
    cout << "Testing LRU cache basic operations..." << endl;
    
    lru_cache<string, string> cache(3); // Max size 3
    
    // Test empty cache
    assert(cache.size() == 0);
    assert(!cache.get("key1").has_value());
    cout << "✓ Empty cache works correctly" << endl;
    
    // Test insertion
    cache.put("key1", "value1");
    assert(cache.size() == 1);
    auto value = cache.get("key1");
    assert(value.has_value());
    assert(value.value() == "value1");
    cout << "✓ Basic insertion and retrieval work" << endl;
    
    // Test multiple insertions
    cache.put("key2", "value2");
    cache.put("key3", "value3");
    assert(cache.size() == 3);
    cout << "✓ Multiple insertions work" << endl;
}

void test_lru_eviction() {
    cout << "\nTesting LRU cache eviction..." << endl;
    
    lru_cache<string, string> cache(2); // Max size 2
    
    // Fill cache
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    assert(cache.size() == 2);
    
    // Add third item - should evict oldest
    cache.put("key3", "value3");
    assert(cache.size() == 2);
    
    // key1 should be evicted (oldest)
    assert(!cache.get("key1").has_value());
    assert(cache.get("key2").has_value());
    assert(cache.get("key3").has_value());
    cout << "✓ LRU eviction works correctly" << endl;
}

void test_lru_update() {
    cout << "\nTesting LRU cache updates..." << endl;
    
    lru_cache<string, string> cache(2);
    
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    
    // Access key1 to make it most recently used
    cache.get("key1");
    
    // Add new item - key2 should be evicted (not key1)
    cache.put("key3", "value3");
    
    assert(cache.get("key1").has_value());
    assert(!cache.get("key2").has_value());
    assert(cache.get("key3").has_value());
    cout << "✓ LRU update on access works correctly" << endl;
    
    // Test update existing key
    cache.put("key1", "updated_value1");
    auto value = cache.get("key1");
    assert(value.has_value());
    assert(value.value() == "updated_value1");
    cout << "✓ Key update works correctly" << endl;
}

void test_lru_removal() {
    cout << "\nTesting LRU cache removal..." << endl;
    
    lru_cache<string, string> cache(3);
    
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");
    assert(cache.size() == 3);
    
    // Remove middle key
    cache.remove("key2");
    assert(cache.size() == 2);
    assert(!cache.get("key2").has_value());
    assert(cache.get("key1").has_value());
    assert(cache.get("key3").has_value());
    cout << "✓ Key removal works correctly" << endl;
    
    // Remove non-existent key
    cache.remove("nonexistent");
    assert(cache.size() == 2);
    cout << "✓ Removing non-existent key is safe" << endl;
}

void test_lru_get_all_keys() {
    cout << "\nTesting LRU cache get_all_keys..." << endl;
    
    lru_cache<string, string> cache(5);
    
    cache.put("key3", "value3");
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    
    auto keys = cache.get_all_keys();
    assert(keys.size() == 3);
    
    // Keys should be in order of insertion (most recent first)
    assert(keys[0] == "key2");
    assert(keys[1] == "key1");
    assert(keys[2] == "key3");
    cout << "✓ get_all_keys returns correct order" << endl;
}

int main() {
    cout << "=== LRU Cache Tests ===" << endl;
    
    try {
        test_lru_basic();
        test_lru_eviction();
        test_lru_update();
        test_lru_removal();
        test_lru_get_all_keys();
        
        cout << "\n✅ All LRU cache tests passed!" << endl;
        return 0;
    } catch (const exception& e) {
        cout << "\n❌ Test failed: " << e.what() << endl;
        return 1;
    }
}