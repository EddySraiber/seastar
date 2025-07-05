# Test Makefile for KV Server
# Run from tests/ directory: make

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wno-unused-parameter -g
SEASTAR_INCLUDE = -I/home/eddy/ScyllaDB/seastar/include -I/home/eddy/ScyllaDB/seastar/build/release/gen/include -I..
SEASTAR_DEFINES = -DSEASTAR_API_LEVEL=7 -DSEASTAR_SSTRING -DSEASTAR_DEPRECATED_OSTREAM_FORMATTERS -DSEASTAR_LOGGER_COMPILE_TIME_FMT -DSEASTAR_SCHEDULING_GROUPS_COUNT=16 -DSEASTAR_LOGGER_TYPE_STDOUT -DFMT_SHARED -I/usr/include/p11-kit-1
SEASTAR_LIBS = /home/eddy/ScyllaDB/seastar/build/release/libseastar.a \
	/usr/lib/x86_64-linux-gnu/libboost_program_options.so \
	/usr/lib/x86_64-linux-gnu/libboost_thread.so \
	/usr/lib/x86_64-linux-gnu/libcares.so \
	/usr/lib/x86_64-linux-gnu/libfmt.so.9.1.0 \
	-ldl -lsctp -latomic -llz4 -lgnutls -lgmp -lunistring -lnettle -lhogweed -ltasn1 -lidn2 -lp11-kit -lprotobuf -lz -lhwloc -lm -ludev -lpthread -lyaml-cpp

# Parent directory files
SRC_DIR = ..
KV_STORE_SOURCES = $(SRC_DIR)/kv_store.cc
KV_STORE_HEADERS = $(SRC_DIR)/kv_store.hh

.PHONY: all clean test_utils test_lru test_encoding test run

all: test_utils test_lru test_encoding

# Test utility functions (JSON escaping, URL encoding)
test_utils: test_utils.cc $(KV_STORE_SOURCES) $(KV_STORE_HEADERS)
	$(CXX) $(CXXFLAGS) $(SEASTAR_INCLUDE) $(SEASTAR_DEFINES) test_utils.cc $(KV_STORE_SOURCES) -o test_utils $(SEASTAR_LIBS)

# Test LRU cache (header-only, no need for kv_store.cc)
test_lru: test_lru.cc $(KV_STORE_HEADERS)
	$(CXX) $(CXXFLAGS) $(SEASTAR_INCLUDE) $(SEASTAR_DEFINES) test_lru.cc -o test_lru $(SEASTAR_LIBS)

# Test encoding functions
test_encoding: test_encoding.cc $(KV_STORE_SOURCES) $(KV_STORE_HEADERS)
	$(CXX) $(CXXFLAGS) $(SEASTAR_INCLUDE) $(SEASTAR_DEFINES) test_encoding.cc $(KV_STORE_SOURCES) -o test_encoding $(SEASTAR_LIBS)

# Run all tests
test: all
	@echo "=== Running Utility Tests ==="
	./test_utils
	@echo ""
	@echo "=== Running LRU Cache Tests ==="
	./test_lru
	@echo ""
	@echo "=== Running Encoding Tests ==="
	./test_encoding
	@echo ""
	@echo "✅ All tests completed!"

# Alias for test
run: test

clean:
	rm -f test_utils test_lru test_encoding *.o

help:
	@echo "KV Server Test Suite"
	@echo "===================="
	@echo "Available targets:"
	@echo "  all          - Build all tests"
	@echo "  test_utils   - Build utility function tests"
	@echo "  test_lru     - Build LRU cache tests"  
	@echo "  test_encoding- Build encoding function tests"
	@echo "  test (or run)- Build and run all tests"
	@echo "  clean        - Remove test binaries"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Usage examples:"
	@echo "  make test    - Run all tests"
	@echo "  make clean   - Clean up"