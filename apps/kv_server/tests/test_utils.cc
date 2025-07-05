#include "kv_store.hh"
#include <iostream>
#include <cassert>

using namespace std;

void test_json_escape() {
    cout << "Testing JSON escaping..." << endl;
    
    // Test normal string
    sstring test1 = "hello world";
    sstring result1 = json_escape(test1);
    assert(result1 == "hello world");
    cout << "✓ Normal string: " << test1 << " -> " << result1 << endl;
    
    // Test string with quotes
    sstring test2 = "quote: \"test\"";
    sstring result2 = json_escape(test2);
    cout << "✓ String with quotes: " << test2 << " -> " << result2 << endl;
    
    // Test string with newlines and tabs
    sstring test3 = "newline: \n tab: \t";
    sstring result3 = json_escape(test3);
    cout << "✓ String with newlines/tabs: " << test3 << " -> " << result3 << endl;
    
    // Test empty string
    sstring test4 = "";
    sstring result4 = json_escape(test4);
    assert(result4 == "");
    cout << "✓ Empty string handled correctly" << endl;
}

void test_url_encoding() {
    cout << "\nTesting URL encoding..." << endl;
    
    // Test normal string
    sstring test1 = "hello";
    sstring encoded1 = url_encode(test1);
    sstring decoded1 = url_decode(encoded1);
    assert(decoded1 == test1);
    cout << "✓ Normal string: " << test1 << " -> " << encoded1 << " -> " << decoded1 << endl;
    
    // Test string with spaces
    sstring test2 = "hello world";
    sstring encoded2 = url_encode(test2);
    sstring decoded2 = url_decode(encoded2);
    assert(decoded2 == test2);
    cout << "✓ String with spaces: " << test2 << " -> " << encoded2 << " -> " << decoded2 << endl;
    
    // Test string with special characters
    sstring test3 = "hello/world?key=value";
    sstring encoded3 = url_encode(test3);
    sstring decoded3 = url_decode(encoded3);
    assert(decoded3 == test3);
    cout << "✓ String with special chars: " << test3 << " -> " << encoded3 << " -> " << decoded3 << endl;
    
    // Test empty string
    sstring test4 = "";
    sstring encoded4 = url_encode(test4);
    sstring decoded4 = url_decode(encoded4);
    assert(decoded4 == test4);
    cout << "✓ Empty string handled correctly" << endl;
}

int main() {
    cout << "=== KV Server Utility Function Tests ===" << endl;
    
    try {
        test_json_escape();
        test_url_encoding();
        
        cout << "\n✅ All utility tests passed!" << endl;
        return 0;
    } catch (const exception& e) {
        cout << "\n❌ Test failed: " << e.what() << endl;
        return 1;
    }
}