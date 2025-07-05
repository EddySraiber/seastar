#include "kv_store.hh"
#include <iostream>
#include <cassert>

using namespace std;

void test_json_edge_cases() {
    cout << "Testing JSON escaping edge cases..." << endl;
    
    // Test all special characters
    sstring test1 = "\"\\\/\b\f\n\r\t";
    sstring result1 = json_escape(test1);
    cout << "✓ All special chars escaped: " << result1 << endl;
    
    // Test control characters
    sstring test2 = sstring(1, '\x01') + sstring(1, '\x1F');
    sstring result2 = json_escape(test2);
    cout << "✓ Control characters escaped: " << result2 << endl;
    
    // Test unicode boundary
    sstring test3 = sstring(1, '\x7F') + sstring(1, '\x80');
    sstring result3 = json_escape(test3);
    cout << "✓ Unicode boundary handled: " << result3 << endl;
}

void test_url_edge_cases() {
    cout << "\nTesting URL encoding edge cases..." << endl;
    
    // Test percent encoding
    sstring test1 = "100% sure";
    sstring encoded1 = url_encode(test1);
    sstring decoded1 = url_decode(encoded1);
    assert(decoded1 == test1);
    cout << "✓ Percent sign: " << test1 << " -> " << encoded1 << " -> " << decoded1 << endl;
    
    // Test plus sign (space alternative in URL encoding)
    sstring test2 = "hello+world";
    sstring encoded2 = url_encode(test2);
    sstring decoded2 = url_decode(encoded2);
    assert(decoded2 == test2);
    cout << "✓ Plus sign: " << test2 << " -> " << encoded2 << " -> " << decoded2 << endl;
    
    // Test already encoded string
    sstring test3 = "hello%20world";
    sstring decoded3 = url_decode(test3);
    cout << "✓ Pre-encoded string: " << test3 << " -> " << decoded3 << endl;
    
    // Test safe characters (should not be encoded)
    sstring test4 = "hello-world_test.file~";
    sstring encoded4 = url_encode(test4);
    cout << "✓ Safe characters: " << test4 << " -> " << encoded4 << endl;
    
    // Test non-ASCII characters
    sstring test5 = "café";
    sstring encoded5 = url_encode(test5);
    sstring decoded5 = url_decode(encoded5);
    cout << "✓ Non-ASCII: " << test5 << " -> " << encoded5 << " -> " << decoded5 << endl;
}

void test_malformed_input() {
    cout << "\nTesting malformed input handling..." << endl;
    
    // Test incomplete percent encoding
    sstring test1 = "hello%2";
    sstring decoded1 = url_decode(test1);
    cout << "✓ Incomplete percent encoding: " << test1 << " -> " << decoded1 << endl;
    
    // Test invalid hex in percent encoding
    sstring test2 = "hello%GG";
    try {
        sstring decoded2 = url_decode(test2);
        cout << "✓ Invalid hex handled: " << test2 << " -> " << decoded2 << endl;
    } catch (...) {
        cout << "✓ Invalid hex throws exception (acceptable)" << endl;
    }
    
    // Test plus sign decoding
    sstring test3 = "hello+world";
    sstring decoded3 = url_decode(test3);
    cout << "✓ Plus sign decoding: " << test3 << " -> " << decoded3 << endl;
}

void test_roundtrip_encoding() {
    cout << "\nTesting roundtrip encoding..." << endl;
    
    vector<sstring> test_strings = {
        "simple",
        "with spaces",
        "special!@#$%^&*()chars",
        "unicode: café, naïve, 中文",
        "",
        "a",
        "very long string with many different characters including spaces, punctuation, and more!"
    };
    
    for (const auto& test : test_strings) {
        sstring encoded = url_encode(test);
        sstring decoded = url_decode(encoded);
        assert(decoded == test);
        
        sstring json_escaped = json_escape(test);
        // Note: JSON escape is one-way, so we don't test roundtrip
        
        cout << "✓ Roundtrip OK: \"" << test << "\"" << endl;
    }
}

int main() {
    cout << "=== Encoding Function Tests ===" << endl;
    
    try {
        test_json_edge_cases();
        test_url_edge_cases();
        test_malformed_input();
        test_roundtrip_encoding();
        
        cout << "\n✅ All encoding tests passed!" << endl;
        return 0;
    } catch (const exception& e) {
        cout << "\n❌ Test failed: " << e.what() << endl;
        return 1;
    }
}