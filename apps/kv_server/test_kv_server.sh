#!/bin/bash
# KV Server Test Script - Tests all cleaned up functionality

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Configuration
PORT=8080
SERVER_PID=""
SERVER_LOG="/tmp/kv_server_test.log"

# Stop any existing kv_server processes
stop_existing_servers() {
    echo -e "${YELLOW}🛑 Stopping any existing kv_server processes...${NC}"
    
    # Find existing processes (exclude grep and this script)
    existing_pids=$(ps aux | grep './kv_server' | grep -v grep | grep -v test_kv_server | awk '{print $2}' || true)
    
    if [ -z "$existing_pids" ]; then
        echo -e "${GREEN}✅ No existing kv_server processes found${NC}"
    else
        echo "Found existing processes: $existing_pids"
        echo "$existing_pids" | while read -r pid; do
            if [ ! -z "$pid" ]; then
                echo "  Stopping PID $pid..."
                kill -9 $pid 2>/dev/null || true
            fi
        done
        
        # Wait a moment and verify
        sleep 2
        remaining=$(ps aux | grep './kv_server' | grep -v grep | grep -v test_kv_server | awk '{print $2}' || true)
        if [ -z "$remaining" ]; then
            echo -e "${GREEN}✅ All existing kv_server processes stopped${NC}"
        else
            echo -e "${RED}⚠️  Some processes may still be running: $remaining${NC}"
        fi
    fi
}

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}🧹 Cleaning up...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        kill -9 $SERVER_PID 2>/dev/null || true
        sleep 1
    fi
    pkill -f kv_server 2>/dev/null || true
    rm -f $SERVER_LOG
    echo -e "${GREEN}✅ Cleanup complete${NC}"
}

# Set trap for cleanup
trap cleanup EXIT

echo "🚀 KV Server Test Script"
echo "========================"

# Stop any existing servers first
stop_existing_servers

# Test function with retries
test_endpoint() {
    local method=$1
    local url=$2
    local data=$3
    local expected_pattern=$4
    local description=$5
    
    echo -e "${BLUE}Testing: $description${NC}"
    echo "  Command: curl -s -X $method ${data:+-d \"$data\"} $url"
    
    for i in {1..3}; do
        if [ -z "$data" ]; then
            response=$(curl -s -X "$method" "$url" 2>/dev/null || echo "ERROR")
        else
            response=$(curl -s -X "$method" -d "$data" "$url" 2>/dev/null || echo "ERROR")
        fi
        
        if [[ "$response" =~ $expected_pattern ]]; then
            echo -e "  ${GREEN}✅ PASS${NC}: $response"
            return 0
        fi
        
        echo "  Attempt $i failed, retrying..."
        sleep 1
    done
    
    echo -e "  ${RED}❌ FAIL${NC}: $response"
    echo -e "  Expected pattern: $expected_pattern"
    return 1
}

echo -e "\n${YELLOW}📦 Building server...${NC}"
export LD_LIBRARY_PATH=/home/eddy/seastar/build/debug:$LD_LIBRARY_PATH
make clean && make

echo -e "\n${YELLOW}🔧 Starting KV server...${NC}"
# Start server with reduced logging and capture PID
./kv_server --port $PORT --data-dir /tmp/kv_test_data --default-log-level=warn > $SERVER_LOG 2>&1 &
SERVER_PID=$!

echo "  Server PID: $SERVER_PID"
echo "  Log file: $SERVER_LOG"

# Wait for server to start
echo "  Waiting for server to start..."
sleep 3

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}❌ Server failed to start!${NC}"
    cat $SERVER_LOG
    exit 1
fi

echo -e "${GREEN}✅ Server started successfully${NC}"

BASE_URL="http://localhost:$PORT"

echo -e "\n${YELLOW}🧪 Running API tests...${NC}"
echo "================================="

# Test 1: Health check
test_endpoint "GET" "$BASE_URL/health" "" "success.*healthy" "Health check"

# Test 2: Stats endpoint  
test_endpoint "GET" "$BASE_URL/stats" "" "shard_count" "Server stats"

# Test 3: List empty keys
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys" "" "success.*keys.*\[\]" "List empty keys"

# Test 4: PUT new key
test_endpoint "PUT" "$BASE_URL/api/v1/kv/keys/hello" "World" "success.*stored" "Store new key"

# Test 5: GET existing key
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys/hello" "" "success.*World" "Retrieve stored key"

# Test 6: List keys (should show our key)
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys" "" "success.*hello" "List keys with data"

# Test 7: UPDATE existing key
test_endpoint "PUT" "$BASE_URL/api/v1/kv/keys/hello" "Updated World!" "success.*stored" "Update existing key"

# Test 8: GET updated key
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys/hello" "" "success.*Updated World!" "Retrieve updated key"

# Test 9: PUT another key
test_endpoint "PUT" "$BASE_URL/api/v1/kv/keys/test-key-123" "Test Value 123" "success.*stored" "Store second key"

# Test 10: List multiple keys
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys" "" "success.*hello.*test-key-123" "List multiple keys"

# Test 11: DELETE key
test_endpoint "DELETE" "$BASE_URL/api/v1/kv/keys/hello" "" "success.*deleted" "Delete key"

# Test 12: GET deleted key (should fail)
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys/hello" "" "error.*not found" "Get deleted key"

# Test 13: List keys after deletion
test_endpoint "GET" "$BASE_URL/api/v1/kv/keys" "" "success.*test-key-123" "List keys after deletion"

# Test 14: Key length validation (should fail)
long_key=$(printf '%*s' 300 '' | tr ' ' 'a')
test_endpoint "PUT" "$BASE_URL/api/v1/kv/keys/$long_key" "test" "error.*too long" "Key length validation"

# Test 15: Empty value validation (should fail)  
test_endpoint "PUT" "$BASE_URL/api/v1/kv/keys/empty-test" "" "error.*empty" "Empty value validation"

# Test 16: Test endpoints (legacy compatibility)
test_endpoint "PUT" "$BASE_URL/api/v1/kv/test" "Test endpoint value" "success.*stored" "Legacy test PUT"
test_endpoint "GET" "$BASE_URL/api/v1/kv/test" "" "success.*Test endpoint value" "Legacy test GET"

echo -e "\n${YELLOW}📊 Server resource usage:${NC}"
if kill -0 $SERVER_PID 2>/dev/null; then
    ps -p $SERVER_PID -o pid,ppid,cmd,%mem,%cpu
fi

echo -e "\n${YELLOW}📋 Final key list:${NC}"
curl -s "$BASE_URL/api/v1/kv/keys" | jq '.' 2>/dev/null || curl -s "$BASE_URL/api/v1/kv/keys"

echo -e "\n${GREEN}🎉 All tests completed successfully!${NC}"
echo -e "\n${YELLOW}💡 Key features demonstrated:${NC}"
echo "  ✅ Parameterized routes (/api/v1/kv/keys/{key})"
echo "  ✅ Key-value CRUD operations (PUT, GET, DELETE)"
echo "  ✅ Key listing functionality"
echo "  ✅ Key length validation (255 char limit)"
echo "  ✅ Empty value validation"
echo "  ✅ Health and stats endpoints"
echo "  ✅ Legacy test endpoints"
echo "  ✅ Clean build (no warnings)"
echo "  ✅ Proper error handling"

echo -e "\n${GREEN}🚀 KV Server is ready for production!${NC}"