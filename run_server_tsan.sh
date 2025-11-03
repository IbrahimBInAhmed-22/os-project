#!/bin/bash
make clean
make

# Clean data
rm -rf users users.txt

# Run with Helgrind
valgrind --tool=helgrind \
         --log-file=helgrind.log \
         ./server
