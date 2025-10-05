# Dropbox Clone Server - Phase 1

A multi-threaded file storage server implementing producer-consumer architecture with thread pools.

## Architecture Overview

The server uses a three-layer architecture:

1. **Main Thread (Accept Layer)**
   - Accepts incoming TCP connections
   - Pushes socket descriptors into Client Queue

2. **Client Thread Pool (Communication Layer)**
   - Consumes Client Queue
   - Handles authentication (signup/login)
   - Receives commands from clients
   - Creates tasks and enqueues them to Task Queue
   - Waits for task completion

3. **Worker Thread Pool (Execution Layer)**
   - Consumes Task Queue
   - Executes file operations (UPLOAD, DOWNLOAD, DELETE, LIST)
   - Updates user quotas
   - Signals completion back to client threads

### Synchronization Strategy

- **Client Queue**: Protected by mutex, uses condition variable for blocking dequeue
- **Task Queue**: Protected by mutex, uses condition variable for blocking dequeue
- **User Database**: Protected by mutex for all read/write operations
- **Task Completion**: Each task has its own mutex and condition variable for worker→client communication

## Building the Project

```bash
# Build both server and client
make

# Clean build artifacts
make clean

# Build with ThreadSanitizer (race detection)
make tsan

# Run with Valgrind (memory leak detection)
make valgrind
```

## Running the Server

```bash
./server
```

The server will:
- Listen on port 8080
- Create 5 client threads
- Create 3 worker threads
- Create `./storage/` directory for file storage

Press `Ctrl+C` to gracefully shutdown the server.

## Running the Client

```bash
./client
```

The client will connect to `localhost:8080` and prompt for authentication.

### Commands

1. **Authentication** (first step):
   ```
   SIGNUP username password    # Create new account
   LOGIN username password     # Login to existing account
   ```

2. **File Operations**:
   ```
   UPLOAD filename    # Upload a file from current directory
   DOWNLOAD filename  # Download a file to current directory
   DELETE filename    # Delete a file from server
   LIST              # List all your files
   QUIT              # Disconnect
   ```

## Testing Phase 1

### Manual Test Steps

1. **Start the server**:
   ```bash
   ./server
   ```

2. **In another terminal, run client**:
   ```bash
   ./client
   ```

3. **Create a test file**:
   ```bash
   echo "Hello World" > test.txt
   ```

4. **Test signup and upload**:
   ```
   > SIGNUP testuser pass123
   > UPLOAD test.txt
   > LIST
   ```

5. **Test download**:
   ```
   > DOWNLOAD test.txt
   ```

6. **Test delete**:
   ```
   > DELETE test.txt
   > LIST
   ```

### Automated Test Script

Run the included test script:
```bash
chmod +x test_phase1.sh
./test_phase1.sh
```

## Memory Leak Testing

```bash
# Terminal 1: Run server with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./server

# Terminal 2: Run client and perform operations
./client
# ... perform some operations ...
# QUIT

# Terminal 1: Ctrl+C to stop server
# Check Valgrind output for leaks
```

## Race Condition Testing

```bash
# Build with ThreadSanitizer
make tsan

# Run server
./server

# Run multiple clients simultaneously
./client &
./client &
./client &

# Perform concurrent operations
# Check for race condition warnings
```

## File Structure

```
.
├── server.h           # Header file with structures and prototypes
├── server.c           # Main server and accept loop
├── queue.c            # Thread-safe queue implementations
├── user.c             # User authentication and quota management
├── worker.c           # Worker thread and file operation handlers
├── client_thread.c    # Client thread and command processing
├── utils.c            # Utility functions
├── client.c           # Test client program
├── Makefile           # Build configuration
└── README.md          # This file
```

## Features Implemented

✅ Multi-threaded server with thread pools  
✅ Producer-consumer queues with proper synchronization  
✅ User signup/login with password protection  
✅ Per-user storage quota (10MB default)  
✅ File upload/download/delete/list operations  
✅ Graceful shutdown (Ctrl+C handling)  
✅ No busy waiting (using condition variables)  
✅ Memory leak prevention  
✅ Race condition prevention  

## Known Limitations (Phase 1)

- Single client per user at a time (Phase 2 will handle concurrent sessions)
- No file locking (Phase 2 will add conflict resolution)
- Basic error handling
- No encryption (files stored in plaintext)
- No persistent user database (users lost on restart)

## Design Decisions

### Worker→Client Communication

**Chosen Approach**: Task-specific condition variables

Each task carries its own mutex and condition variable. The client thread waits on this condition variable after enqueuing the task. When the worker completes the task, it signals the condition variable, waking up the specific client thread.

**Rationale**:
- Simple and straightforward for Phase 1
- No complex routing needed
- Client thread blocks efficiently (no busy waiting)
- Each task is independent

**Trade-offs**:
- Allocates synchronization primitives per task (memory overhead)
- Client thread stays blocked (but acceptable for Phase 1)
- Will need enhancement for Phase 2 to handle multiple clients per user

## Quota Management

- Each user has a 10MB quota
- Quota is updated atomically after successful file operations
- Upload checks quota before accepting file
- File replacement (uploading same filename) correctly adjusts quota

## Next Steps (Phase 2)

- [ ] Support multiple concurrent clients per user
- [ ] Implement per-file locking for conflict resolution
- [ ] Enhanced worker→client communication
- [ ] Robust concurrency testing
- [ ] Complete Valgrind + ThreadSanitizer validation
- [ ] Persistent user database
