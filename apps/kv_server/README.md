# KV Server

A high-performance key-value store built with Seastar providing REST API for storing, retrieving, and managing key-value pairs with persistent storage and LRU caching.

## Building

### Prerequisites

- C++23 compatible compiler (GCC 10+ or Clang 11+)
- Seastar framework
- pkg-config
- CMake 3.5+ (for CMake build) or Make

### ⚠️ Important: System Configuration Required

**Before running the server, you MUST increase the system's async I/O limits, or the server will crash on startup.**

Run this command to fix the issue:
```bash
sudo sysctl -w fs.aio-max-nr=1048576
```

To make this permanent across reboots:
```bash
echo 'fs.aio-max-nr = 1048576' | sudo tee -a /etc/sysctl.conf
```

**Error you'll see if you skip this step:**
```
terminate called after throwing an instance of 'std::runtime_error'
  what():  Could not setup Async I/O: Resource temporarily unavailable
```

### Build and Run

```bash
# Build
make

# Run server
./kv_server

# Run tests
make test

# Clean
make clean
```

## Usage

### Start Server
```bash
./kv_server
```

### Configuration
- `--port 8080` (default)
- `--bind-address 127.0.0.1` (default)
- `--data-dir /tmp/kv_store` (default)
- `--cache-size 1000` (default)

## API Endpoints

**Base URL:** `http://localhost:8080`

- `PUT /api/v1/kv/keys/{key}` - Store value
- `GET /api/v1/kv/keys/{key}` - Get value
- `DELETE /api/v1/kv/keys/{key}` - Delete key
- `GET /api/v1/kv/keys` - List all keys
- `GET /health` - Health check
- `GET /stats` - Server stats

**Examples:**
```bash
curl -X PUT -d "Hello" http://localhost:8080/api/v1/kv/keys/greeting
curl http://localhost:8080/api/v1/kv/keys/greeting
curl http://localhost:8080/health
```


## Testing

### Unit Tests
```bash
# Run all unit tests
make test

# Run specific test
make -C tests test_lru
```

### Manual Testing
```bash
# Health check
curl http://localhost:8080/health

# Basic functionality
curl -X PUT -d "test_value" http://localhost:8080/api/v1/kv/keys/test_key
curl http://localhost:8080/api/v1/kv/keys/test_key
curl http://localhost:8080/api/v1/kv/keys
curl -X DELETE http://localhost:8080/api/v1/kv/keys/test_key
```

## Current Limitations

- No authentication or security
- REST API only
- Single-key operations only
- Fixed sharding strategy

See `LeftUndone.md` for planned improvements.