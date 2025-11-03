#!/bin/bash

echo "========================================="
echo "THREAD SANITIZER (Race Condition Test)"
echo "========================================="

# Build client first (normal build)
echo "Building client..."
gcc -Wall -Wextra -pthread -O2 -c client.c -o client.o
gcc -pthread -o client client.o

# Build server with ThreadSanitizer
echo "Building server with ThreadSanitizer..."
gcc -Wall -Wextra -pthread -g -fsanitize=thread \
    -o server_tsan server.c queue.c threadpool.c utils.c

# Clean old data
rm -rf users users.txt

echo ""
echo "Starting server with ThreadSanitizer..."
./server_tsan 2>&1 | tee tsan_output.txt &
SERVER_PID=$!

sleep 3

echo "Running 5 concurrent test clients..."

# Run 5 clients concurrently
for i in {1..5}; do
    {
        echo "REGISTER user$i pass$i"
        sleep 0.3
        echo "LOGIN user$i pass$i"
        sleep 0.3
        echo "UPLOAD utils.h"
        sleep 0.5
        echo "LIST"
        sleep 0.3
        echo "DELETE utils.h"
        sleep 0.3
        echo "QUIT"
    } | ./client > /dev/null 2>&1 &
done

# Wait for all clients to finish
wait

sleep 2

echo ""
echo "Shutting down server..."
kill -INT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "========================================="
echo "ThreadSanitizer Results:"
echo "========================================="

if grep -q "ThreadSanitizer: data race" tsan_output.txt; then
    echo "❌ RACE CONDITIONS DETECTED!"
    echo ""
    grep -A 10 "ThreadSanitizer: data race" tsan_output.txt
elif grep -q "ThreadSanitizer: reported" tsan_output.txt; then
    echo "✅ Test completed - Check tsan_output.txt for details"
    tail -20 tsan_output.txt
else
    echo "✅ NO RACE CONDITIONS DETECTED!"
    echo "Server ran successfully with ThreadSanitizer"
fi

echo ""
echo "Full report saved in: tsan_output.txt"

