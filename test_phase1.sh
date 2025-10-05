#!/bin/bash

# Automated test script for Phase 1
# Tests basic functionality with a single client

echo "=== Dropbox Clone Phase 1 Test Script ==="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to print test result
test_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASSED${NC}: $2"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAILED${NC}: $2"
        ((TESTS_FAILED++))
    fi
}

# Check if binaries exist
echo "Step 1: Checking if binaries are built..."
if [ ! -f "./server" ] || [ ! -f "./client" ]; then
    echo -e "${RED}Error: server or client not found. Run 'make' first.${NC}"
    exit 1
fi
test_result 0 "Binaries exist"
echo ""

# Clean up any previous test files
echo "Step 2: Cleaning up previous test files..."
rm -rf ./storage
rm -f test_*.txt downloaded_*.txt
echo "Done"
echo ""

# Start the server in background
echo "Step 3: Starting server..."
./server > server.log 2>&1 &
SERVER_PID=$!
sleep 2

# Check if server is running
if ps -p $SERVER_PID > /dev/null; then
    test_result 0 "Server started (PID: $SERVER_PID)"
else
    test_result 1 "Server failed to start"
    exit 1
fi
echo ""

# Create test files
echo "Step 4: Creating test files..."
echo "This is test file 1" > test1.txt
echo "This is a larger test file with more content for testing" > test2.txt
dd if=/dev/urandom of=test3.bin bs=1024 count=100 2>/dev/null
test_result 0 "Test files created"
echo ""

# Test function using netcat or telnet
test_with_nc() {
    local test_name=$1
    shift
    local commands="$@"
    
    echo "$commands" | nc localhost 8080 > test_output.txt 2>&1 &
    NC_PID=$!
    sleep 1
    
    if ps -p $NC_PID > /dev/null; then
        kill $NC_PID 2>/dev/null
    fi
}

# Function to send commands via expect (if available)
if command -v expect > /dev/null; then
    echo "Step 5: Running automated tests with expect..."
    
    # Test 1: Signup
    expect << 'EOF' > /dev/null 2>&1
set timeout 5
spawn ./client
expect "Enter command"
send "SIGNUP testuser password123\r"
expect "OK:"
send "LIST\r"
expect "No files"
send "QUIT\r"
expect eof
EOF
    test_result $? "User signup"
    
    # Test 2: Upload
    expect << 'EOF' > /dev/null 2>&1
set timeout 5
spawn ./client
expect "Enter command"
send "LOGIN testuser password123\r"
expect "OK:"
send "UPLOAD test1.txt\r"
expect "OK:"
send "QUIT\r"
expect eof
EOF
    test_result $? "File upload"
    
    # Test 3: List
    expect << 'EOF' > /dev/null 2>&1
set timeout 5
spawn ./client
expect "Enter command"
send "LOGIN testuser password123\r"
expect "OK:"
send "LIST\r"
expect "test1.txt"
send "QUIT\r"
expect eof
EOF
    test_result $? "File listing"
    
    # Test 4: Download
    rm -f test1.txt  # Remove local copy
    expect << 'EOF' > /dev/null 2>&1
set timeout 5
spawn ./client
expect "Enter command"
send "LOGIN testuser password123\r"
expect "OK:"
send "DOWNLOAD test1.txt\r"
expect "OK:"
send "QUIT\r"
expect eof
EOF
    test_result $? "File download"
    
    # Verify downloaded file exists
    if [ -f "test1.txt" ]; then
        test_result 0 "Downloaded file exists"
    else
        test_result 1 "Downloaded file exists"
    fi
    
    # Test 5: Delete
    expect << 'EOF' > /dev/null 2>&1
set timeout 5
spawn ./client
expect "Enter command"
send "LOGIN testuser password123\r"
expect "OK:"
send "DELETE test1.txt\r"
expect "OK:"
send "LIST\r"
expect "No files"
send "QUIT\r"
expect eof
EOF
    test_result $? "File deletion"
    
    # Test 6: Multiple uploads
    expect << 'EOF' > /dev/null 2>&1
set timeout 10
spawn ./client
expect "Enter command"
send "LOGIN testuser password123\r"
expect "OK:"
send "UPLOAD test1.txt\r"
expect "OK:"
send "UPLOAD test2.txt\r"
expect "OK:"
send "LIST\r"
expect "test1.txt"
expect "test2.txt"
send "QUIT\r"
expect eof
EOF
    test_result $? "Multiple file uploads"
    
else
    echo -e "${YELLOW}Warning: 'expect' not installed. Skipping automated tests.${NC}"
    echo "Install expect: sudo apt-get install expect"
    echo ""
    echo "Manual testing instructions:"
    echo "1. Run: ./client"
    echo "2. Test: SIGNUP testuser password123"
    echo "3. Test: UPLOAD test1.txt"
    echo "4. Test: LIST"
    echo "5. Test: DOWNLOAD test1.txt"
    echo "6. Test: DELETE test1.txt"
    echo "7. Test: QUIT"
fi

echo ""

# Check server is still running
echo "Step 6: Checking server health..."
if ps -p $SERVER_PID > /dev/null; then
    test_result 0 "Server still running"
else
    test_result 1 "Server crashed during tests"
fi
echo ""

# Graceful shutdown
echo "Step 7: Testing graceful shutdown..."
kill -INT $SERVER_PID
sleep 2

if ps -p $SERVER_PID > /dev/null; then
    test_result 1 "Graceful shutdown (server still running)"
    kill -9 $SERVER_PID
else
    test_result 0 "Graceful shutdown"
fi
echo ""

# Check for memory leaks in server log
echo "Step 8: Checking server log for errors..."
if grep -q "ERROR" server.log; then
    echo -e "${YELLOW}Warning: Errors found in server log${NC}"
    grep "ERROR" server.log
else
    test_result 0 "No errors in server log"
fi
echo ""

# Summary
echo "======================================"
echo "Test Summary:"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo "======================================"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Run memory leak test: make clean && valgrind --leak-check=full ./server"
    echo "2. Run race condition test: make tsan && ./server"
    echo "3. Test with multiple clients manually"
else
    echo -e "${RED}Some tests failed. Check server.log for details.${NC}"
fi

# Cleanup
rm -f test_output.txt

exit $TESTS_FAILED