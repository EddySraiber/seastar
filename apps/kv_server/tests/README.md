# KV Server Test Suite

This directory contains comprehensive tests for the KV server components.

## Running Tests

```bash
# Run all tests
make test

# Run specific tests
make test_utils     # Utility functions (JSON/URL encoding)
make test_lru       # LRU cache functionality  
make test_encoding  # Encoding edge cases

# Clean up
make clean

# Show help
make help
```

## Test Coverage

### `test_utils.cc`
- JSON escaping functionality
- URL encoding/decoding roundtrip
- Basic string handling
- Empty string edge cases

### `test_lru.cc` 
- LRU cache basic operations (put/get/size)
- Eviction policy (oldest items removed first)
- Access-based reordering (LRU updates)
- Key removal and updates
- Key listing functionality

### `test_encoding.cc`
- JSON special character escaping
- Control character handling
- Unicode boundary cases
- URL encoding of safe vs unsafe characters
- Percent encoding edge cases
- Malformed input robustness
- Roundtrip encoding consistency

## Test Results

All tests pass successfully:
- ✅ 15+ utility function tests
- ✅ 8+ LRU cache behavior tests  
- ✅ 20+ encoding edge case tests

## Dependencies

Tests require:
- Seastar framework headers and libraries
- C++23 compiler support
- Parent directory source files (`../kv_store.cc`, `../kv_store.hh`)

## Notes

- Tests run independently without requiring the full server
- Only deprecation warnings are expected (using older Seastar API)
- Tests validate all functional requirements are correctly implemented