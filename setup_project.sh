#!/bin/bash

# Setup script for Dropbox Clone Project
# This script will guide you through setting up all files

echo "=========================================="
echo "  Dropbox Clone Project Setup"
echo "=========================================="
echo ""

# Check if directory exists
if [ -d "dropbox_project" ]; then
    echo "Warning: dropbox_project directory already exists!"
    read -p "Do you want to overwrite it? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 1
    fi
    rm -rf dropbox_project
fi

# Create project directory
echo "Creating project directory..."
mkdir -p dropbox_project
cd dropbox_project

echo ""
echo "=========================================="
echo "IMPORTANT INSTRUCTIONS:"
echo "=========================================="
echo ""
echo "I will now create placeholder files for your project."
echo "You need to copy the code from Claude's artifacts into each file."
echo ""
echo "Steps:"
echo "1. Open each file with: nano filename"
echo "2. Copy the content from Claude's response"
echo "3. Paste it into nano"
echo "4. Save with Ctrl+O, then Enter, then Ctrl+X"
echo ""
read -p "Press Enter to continue..."

# Create all necessary files as placeholders
echo ""
echo "Creating placeholder files..."

touch server.h
touch server.c
touch queue.c
touch user.c
touch worker.c
touch client_thread.c
touch utils.c
touch client.c
touch Makefile
touch README.md
touch DESIGN.md
touch QUICK_START.md
touch test_phase1.sh

chmod +x test_phase1.sh

echo ""
echo "✓ Created the following files:"
ls -1

echo ""
echo "=========================================="
echo "NEXT STEPS:"
echo "=========================================="
echo ""
echo "Copy code into each file in this order:"
echo ""
echo "1. nano server.h           (Header file)"
echo "2. nano queue.c            (Queue implementation)"
echo "3. nano user.c             (User management)"
echo "4. nano worker.c           (Worker threads)"
echo "5. nano client_thread.c    (Client threads)"
echo "6. nano utils.c            (Utility functions)"
echo "7. nano server.c           (Main server)"
echo "8. nano client.c           (Test client)"
echo "9. nano Makefile           (Build file - NO EXTENSION!)"
echo "10. nano README.md         (Documentation)"
echo "11. nano DESIGN.md         (Design doc)"
echo "12. nano QUICK_START.md    (Quick guide)"
echo "13. nano test_phase1.sh    (Test script)"
echo ""
echo "After copying all files, build with:"
echo "  make"
echo ""
echo "Then run:"
echo "  ./server    (in one terminal)"
echo "  ./client    (in another terminal)"
echo ""
echo "=========================================="
echo "Current directory: $(pwd)"
echo "=========================================="