# File Copy Checklist ✅

Use this checklist to make sure you've copied all files correctly.

## Setup Instructions

### 1. Create Project Directory
```bash
mkdir ~/dropbox_project
cd ~/dropbox_project
```

### 2. Copy Files in This Order

Check off each file as you complete it:

- [ ] **server.h** - Header file with all structures
  ```bash
  nano server.h
  # Copy from artifact "server.h - Header File"
  # Save: Ctrl+O, Enter, Ctrl+X
  ```

- [ ] **queue.c** - Thread-safe queue implementation
  ```bash
  nano queue.c
  # Copy from artifact "queue.c - Thread-Safe Queues"
  ```

- [ ] **user.c** - User authentication and quota
  ```bash
  nano user.c
  # Copy from artifact "user.c - User Management"
  ```

- [ ] **worker.c** - Worker threads and file operations
  ```bash
  nano worker.c
  # Copy from artifact "worker.c - Worker Thread Functions"
  ```

- [ ] **client_thread.c** - Client thread handling
  ```bash
  nano client_thread.c
  # Copy from artifact "client_thread.c - Client Thread Functions"
  ```

- [ ] **utils.c** - Utility functions
  ```bash
  nano utils.c
  # Copy from artifact "utils.c - Utility Functions"
  ```

- [ ] **server.c** - Main server code
  ```bash
  nano server.c
  # Copy from artifact "server.c - Main Server"
  ```

- [ ] **client.c** - Test client
  ```bash
  nano client.c
  # Copy from artifact "client.c - Test Client"
  ```

- [ ] **Makefile** - Build configuration (NO .txt extension!)
  ```bash
  nano Makefile
  # Copy from artifact "Makefile"
  # IMPORTANT: Filename must be exactly "Makefile" with capital M
  ```

- [ ] **README.md** - Documentation
  ```bash
  nano README.md
  # Copy from artifact "README.md - Documentation"
  ```

- [ ] **DESIGN.md** - Design document (for submission)
  ```bash
  nano DESIGN.md
  # Copy from artifact "DESIGN.md - Phase 1 Design Document"
  ```

- [ ] **QUICK_START.md** - Quick reference
  ```bash
  nano QUICK_START.md
  # Copy from artifact "QUICK_START.md - Getting Started Guide"
  ```

- [ ] **test_phase1.sh** - Test script
  ```bash
  nano test_phase1.sh
  # Copy from artifact "test_phase1.sh - Automated Test Script"
  chmod +x test_phase1.sh  # Make executable
  ```

---

## Verification Steps

### 3. Verify All Files Exist
```bash
ls -la
```

You should see:
```
-rw-r--r-- client.c
-rw-r--r-- client_thread.c
-rw-r--r-- DESIGN.md
-rw-r--r-- Makefile           ← Must be exactly "Makefile"
-rw-r--r-- queue.c
-rw-r--r-- QUICK_START.md
-rw-r--r-- README.md
-rw-r--r-- server.c
-rw-r--r-- server.h
-rwxr-xr-x test_phase1.sh     ← Should have 'x' (executable)
-rw-r--r-- user.c
-rw-r--r-- utils.c
-rw-r--r-- worker.c
```

### 4. Check File Sizes (shouldn't be empty)
```bash
wc -l *.c *.h Makefile
```

All files should have multiple lines (not 0).

### 5. Test Build
```bash
make
```

Expected output:
```
gcc -Wall -Wextra -pthread -g -c server.c -o server.o
gcc -Wall -Wextra -pthread -g -c queue.c -o queue.o
...
Server built successfully!
Client built successfully!
```

### 6. Verify Executables Created
```bash
ls -lh server client
```

You should see:
```
-rwxr-xr-x server    ← Executable
-rwxr-xr-x client    ← Executable
```

---

## Common Issues & Solutions

### Issue: "make: *** No targets specified and no makefile found"
**Solution:** Makefile is missing or named incorrectly
```bash
ls -la Make*
# Should show "Makefile" not "makefile" or "Makefile.txt"

# Rename if needed:
mv makefile Makefile
# or
mv Makefile.txt Makefile
```

### Issue: "server.h: No such file or directory"
**Solution:** Missing header file
```bash
ls server.h
# If not found, create it with nano and copy content
```

### Issue: Empty files after nano
**Solution:** Make sure to paste content BEFORE saving
1. Open nano: `nano filename`
2. Paste content: Right-click or Ctrl+Shift+V
3. Save: Ctrl+O, Enter
4. Exit: Ctrl+X

### Issue: "Permission denied" when running ./server
**Solution:** File not executable
```bash
chmod +x server client test_phase1.sh
```

---

## Quick Copy Commands (Alternative Method)

If you prefer, you can use `cat` to create files:

```bash
cat > server.h << 'EOF'
# Paste entire server.h content here
# End with EOF on its own line
EOF

cat > queue.c << 'EOF'
# Paste entire queue.c content here
EOF

# Repeat for all files...
```

---

## Using VS Code or Other Editor

If you have VS Code, gedit, or another text editor:

```bash
# Using VS Code
code server.h      # Opens in VS Code

# Using gedit
gedit server.h     # Opens in gedit

# Using vim
vim server.h       # Opens in vim (press 'i' to insert, 'Esc' then ':wq' to save)
```

---

## Final Checklist Before Building

- [ ] All 13 files created
- [ ] No empty files (check with `wc -l *`)
- [ ] Makefile has correct name (capital M, no extension)
- [ ] test_phase1.sh is executable (`chmod +x test_phase1.sh`)
- [ ] All code copied completely (no truncation)
- [ ] In correct directory (`pwd` shows ~/dropbox_project)

---

## Build and Test

```bash
# Clean build
make clean
make

# Should see:
# Server built successfully!
# Client built successfully!

# Quick test
./server &          # Start in background
sleep 2
./client            # Connect as client

# In client:
SIGNUP test pass123
LIST
QUIT

# Stop server
killall server
```

---

## Need Help?

If you're stuck on any file, let me know which one and I'll help you with that specific file!

Good luck, **Ibrahim Bin Ahmed**! 🚀