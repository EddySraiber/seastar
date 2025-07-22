#!/bin/bash

# KV Server Test Commands
# Run these commands to test the KV server endpoints

echo "=== KV Server Test Commands ==="
echo

echo "1. Test health endpoint:"
echo "curl -s http://localhost:8080/health"
echo

echo "2. Test stats endpoint:"
echo "curl -s http://localhost:8080/stats"
echo

echo "3. Test list keys endpoint:"
echo "curl -s http://localhost:8080/api/v1/kv/keys"
echo

echo "4. Test PUT operation:"
echo "curl -s -X PUT http://localhost:8080/api/v1/kv/test"
echo

echo "5. Test GET operation:"
echo "curl -s http://localhost:8080/api/v1/kv/test"
echo

echo "6. Test with data (if you want to add POST body):"
echo "curl -s -X PUT -d 'your_value' http://localhost:8080/api/v1/kv/test"
echo

echo "=== To run all tests automatically ==="
echo "Execute this script with: bash test_commands.sh"
echo

# Uncomment the lines below to run the tests automatically
echo "Running tests..."
curl -s http://localhost:8080/health && echo
curl -s http://localhost:8080/stats && echo  
curl -s http://localhost:8080/api/v1/kv/keys && echo
curl -s -X PUT http://localhost:8080/api/v1/kv/test && echo
curl -s http://localhost:8080/api/v1/kv/test && echo