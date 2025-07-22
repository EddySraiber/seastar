#!/bin/bash
# Test script to verify KV persistence across server restarts

set -e

echo "🔄 Testing KV Server Persistence"
echo "================================"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Configuration
PORT=8080
DATA_DIR="/tmp/kv_persistence_test"
BASE_URL="http://localhost:$PORT"
CLEANUP_FILES=${CLEANUP_FILES:-true}  # Set to false to keep files after test

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}🧹 Cleaning up...${NC}"
    pkill -f kv_server 2>/dev/null || true
    sleep 1
    if [ "$CLEANUP_FILES" = "true" ]; then
        rm -rf $DATA_DIR
        echo -e "${GREEN}✅ Cleanup complete${NC}"
    else
        echo -e "${BLUE}ℹ️  Files preserved at: $DATA_DIR${NC}"
        ls -la $DATA_DIR/
    fi
}

trap cleanup EXIT

# Clean any existing processes
pkill -f kv_server 2>/dev/null || true
sleep 1

# Clean data directory
rm -rf $DATA_DIR
mkdir -p $DATA_DIR

export LD_LIBRARY_PATH=/home/eddy/seastar/build/debug:$LD_LIBRARY_PATH

echo -e "${BLUE}Phase 1: Starting server and adding data${NC}"
echo "============================================"

# Start server
./kv_server --port $PORT --data-dir $DATA_DIR --default-log-level=warn > /dev/null 2>&1 &
SERVER_PID=$!
echo "Started server with PID: $SERVER_PID"

# Wait for server to start
sleep 3

# Add test data
echo "Adding test data..."
curl -s -X PUT -d "Persistent Value 1" $BASE_URL/api/v1/kv/keys/persist-key1
curl -s -X PUT -d "Persistent Value 2" $BASE_URL/api/v1/kv/keys/persist-key2  
curl -s -X PUT -d "Persistent Value 3" $BASE_URL/api/v1/kv/keys/persist-key3

# Verify data was stored
echo "Verifying initial data..."
response1=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key1)
response2=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key2)
response3=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key3)

echo "Key1: $response1"
echo "Key2: $response2" 
echo "Key3: $response3"

# List all keys
all_keys=$(curl -s $BASE_URL/api/v1/kv/keys)
echo "All keys: $all_keys"

echo -e "\n${BLUE}Phase 2: Stopping server${NC}"
echo "========================="

# Stop server gracefully
echo "Stopping server PID: $SERVER_PID"
kill -TERM $SERVER_PID 2>/dev/null || true
sleep 3

# Force kill if still running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "Force killing server"
    kill -9 $SERVER_PID 2>/dev/null || true
    sleep 1
fi

echo "Server stopped"

# Check what files were created
echo -e "\n${YELLOW}Checking persistence files:${NC}"
ls -la $DATA_DIR/
if [ -f $DATA_DIR/kv_log_0.log ]; then
    echo -e "${GREEN}✅ Log file created${NC}"
    echo "Log file contents:"
    cat $DATA_DIR/kv_log_0.log
else
    echo -e "${RED}❌ No log file found${NC}"
fi

echo -e "\n${BLUE}Phase 3: Restarting server${NC}"
echo "============================"

# Start server again
./kv_server --port $PORT --data-dir $DATA_DIR --default-log-level=warn > /dev/null 2>&1 &
SERVER_PID=$!
echo "Restarted server with PID: $SERVER_PID"

# Wait for server to start and load data
sleep 4

echo -e "\n${BLUE}Phase 4: Verifying persistence${NC}"
echo "================================"

# Try to retrieve the previously stored data
echo "Checking if data persisted..."
response1_after=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key1)
response2_after=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key2)
response3_after=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key3)

echo "After restart Key1: $response1_after"
echo "After restart Key2: $response2_after"
echo "After restart Key3: $response3_after"

# List all keys after restart
all_keys_after=$(curl -s $BASE_URL/api/v1/kv/keys)
echo "All keys after restart: $all_keys_after"

# Verify persistence worked
if [[ "$response1_after" == *"Persistent Value 1"* ]] && \
   [[ "$response2_after" == *"Persistent Value 2"* ]] && \
   [[ "$response3_after" == *"Persistent Value 3"* ]]; then
    echo -e "\n${GREEN}🎉 SUCCESS: Persistence is working!${NC}"
    echo -e "${GREEN}✅ All data survived server restart${NC}"
else
    echo -e "\n${RED}❌ FAILURE: Persistence not working properly${NC}"
    echo "Expected data not found after restart"
fi

echo -e "\n${BLUE}Phase 5: Testing updates and deletes${NC}"
echo "====================================="

# Update a key
curl -s -X PUT -d "Updated Persistent Value" $BASE_URL/api/v1/kv/keys/persist-key1
echo "Updated key1"

# Delete a key
curl -s -X DELETE $BASE_URL/api/v1/kv/keys/persist-key2
echo "Deleted key2"

# Add a new key
curl -s -X PUT -d "New Key After Restart" $BASE_URL/api/v1/kv/keys/persist-key4
echo "Added key4"

# Stop and restart again to test these changes
kill -TERM $SERVER_PID 2>/dev/null || true
sleep 3
if kill -0 $SERVER_PID 2>/dev/null; then
    kill -9 $SERVER_PID 2>/dev/null || true
    sleep 1
fi

echo "Restarting server again..."
./kv_server --port $PORT --data-dir $DATA_DIR --default-log-level=warn > /dev/null 2>&1 &
SERVER_PID=$!
sleep 4

# Final verification
final_key1=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key1)
final_key2=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key2)
final_key4=$(curl -s $BASE_URL/api/v1/kv/keys/persist-key4)
final_all=$(curl -s $BASE_URL/api/v1/kv/keys)

echo "Final Key1 (updated): $final_key1"
echo "Final Key2 (deleted): $final_key2"
echo "Final Key4 (new): $final_key4"
echo "Final all keys: $final_all"

if [[ "$final_key1" == *"Updated Persistent Value"* ]] && \
   [[ "$final_key2" == *"not found"* ]] && \
   [[ "$final_key4" == *"New Key After Restart"* ]]; then
    echo -e "\n${GREEN}🎉 FULL SUCCESS: Complete persistence working!${NC}"
    echo -e "${GREEN}✅ Updates, deletes, and new keys all persist correctly${NC}"
else
    echo -e "\n${YELLOW}⚠️  Partial success: Some operations may not be persisting correctly${NC}"
fi

echo -e "\n${YELLOW}Final log file contents:${NC}"
if [ -f $DATA_DIR/kv_log_0.log ]; then
    cat $DATA_DIR/kv_log_0.log
fi