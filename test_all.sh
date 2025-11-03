#!/bin/bash

echo "========================================="
echo "MEMORY LEAK TEST (Valgrind)"
echo "========================================="

# Clean and rebuild
make clean
make

# Start server with valgrind in background
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
         --log-file=valgrind.log ./server &
SERVER_PID=$!

sleep 2

# Run test clients
echo "Running test clients..."
for i in {1..3}; do
    {
        echo "REGISTER testuser$i pass$i"
        sleep 0.3
        echo "LOGIN testuser$i pass$i"
        sleep 0.3
        echo "UPLOAD utils.h"
        sleep 0.5
        echo "LIST"
        sleep 0.3
        echo "DELETE utils.h"
        sleep 0.3
        echo "QUIT"
    } | ./client &
done

wait

# Shutdown server
sleep 2
kill -INT $SERVER_PID
wait $SERVER_PID

echo ""
echo "Valgrind Report:"
cat valgrind.log | grep -A 20 "LEAK SUMMARY"

echo ""
echo "========================================="
echo "RACE CONDITION TEST (ThreadSanitizer)"
echo "========================================="

# Rebuild with TSan
make clean
gcc -Wall -Wextra -pthread -g -fsanitize=thread \
    -o server_tsan server.c queue.c threadpool.c utils.c

# Run with TSan
./server_tsan &
SERVER_PID=$!

sleep 2

# Concurrent stress test
for i in {1..5}; do
    {
        echo "REGISTER racetest$i pass$i"
        sleep 0.2
        echo "LOGIN racetest$i pass$i"
        sleep 0.2
        echo "UPLOAD utils.h"
        sleep 0.3
        echo "UPLOAD queue.c"
        sleep 0.3
        echo "LIST"
        sleep 0.2
        echo "DELETE utils.h"
        sleep 0.2
        echo "LIST"
        sleep 0.2
        echo "QUIT"
    } | ./client &
done

wait

sleep 2
kill -INT $SERVER_PID
wait $SERVER_PID

echo ""
echo "Test complete! Check output above for issues."
