# Quick Start Guide - Dropbox Clone Phase 1

## 🚀 Quick Setup (5 minutes)

### Step 1: Build Everything
```bash
make
```

You should see:
```
Server built successfully!
Client built successfully!
```

### Step 2: Start the Server
```bash
./server
```

You should see:
```
=== Dropbox Clone Server (Phase 1) ===
[Server] Created 5 client threads
[Server] Created 3 worker threads
[Server] Listening on port 8080
[Server] Ready to accept connections. Press Ctrl+C to stop.
```

### Step 3: Connect a Client (in new terminal)
```bash
./client
```

### Step 4: Test Basic Operations
```
Connected to server!
AUTH: Enter SIGNUP username password or LOGIN username password
Enter command: SIGNUP alice pass123

OK: Signup successful

Available commands:
  UPLOAD <filename>
  DOWNLOAD <filename>
  DELETE <filename>
  LIST
  QUIT

> 
```

Create a test file and upload it:
```bash
# In another terminal
echo "Hello World!" > hello.txt
```

Then in client:
```
> UPLOAD hello.txt
OK: File uploaded successfully
> LIST
OK:
hello.txt (13 bytes)
> QUIT
OK: Goodbye
```

---

## 🧠 Understanding the Code Flow

### Analogy: Restaurant Kitchen

Think of the server as a **restaurant kitchen**:

1. **Main Thread = Host/Hostess**
   - Greets customers at the door (accepts connections)
   - Assigns them to available tables (pushes to Client Queue)

2. **Client Threads = Waiters** (5 waiters)
   - Take orders from customers (receive commands)
   - Write order tickets (create Task structures)
   - Put tickets in kitchen window (enqueue to Task Queue)
   - Wait for food to be ready (wait on condition variable)
   - Deliver food to customers (send response)

3. **Worker Threads = Cooks** (3 cooks)
   - Take tickets from kitchen window (dequeue from Task Queue)
   - Prepare the food (execute file operations)
   - Ring the bell when done (signal condition variable)

**Thread-safe queues** = Order window with rules
- Only one person can add/remove orders at a time (mutex)
- Cooks wait when no orders (condition variable)
- Bell rings to wake up cooks when orders arrive (signal)

---

## 🔍 Code Walkthrough

### When a Client Connects:

1. **server.c** (Main thread):
   ```c
   client_socket = accept(...);  // Accept connection
   enqueue_client(&client_queue, client_socket);  // Add to queue
   ```

2. **client_thread.c** (Client thread):
   ```c
   client_socket = dequeue_client(&client_queue);  // Get from queue
   handle_authentication(...);  // LOGIN/SIGNUP
   while (receiving commands) {
       process_command(...);  // Handle each command
   }
   ```

3. **client_thread.c** → `process_command()`:
   ```c
   Task *task = malloc(...);  // Create task
   // ... fill task details ...
   enqueue_task(&task_queue, task);  // Send to workers
   
   pthread_cond_wait(...);  // Wait for completion
   // ... worker signals here ...
   free(task);  // Cleanup
   ```

4. **worker.c** (Worker thread):
   ```c
   task = dequeue_task(&task_queue);  // Get task
   
   switch (task->type) {
       case CMD_UPLOAD: handle_upload(task); break;
       // ... other commands ...
   }
   
   pthread_cond_signal(task->task_cond);  // Signal client thread
   ```

---

## 🔒 Synchronization Points

### Client Queue
```c
// Adding to queue (Main thread)
pthread_mutex_lock(&queue->mutex);
// ... add node ...
pthread_cond_signal(&queue->cond);  // Wake up a client thread
pthread_mutex_unlock(&queue->mutex);

// Taking from queue (Client thread)
pthread_mutex_lock(&queue->mutex);
while (queue empty) {
    pthread_cond_wait(&queue->cond, &queue->mutex);  // Sleep here
}
// ... remove node ...
pthread_mutex_unlock(&queue->mutex);
```

**Why this works:**
- `pthread_cond_wait` releases mutex while sleeping
- When signaled, it wakes up and re-acquires mutex
- No busy waiting!

### Task Completion
```c
// Client thread waiting
pthread_mutex_lock(task->task_mutex);
while (!task->completed) {
    pthread_cond_wait(task->task_cond, task->task_mutex);
}
pthread_mutex_unlock(task->task_mutex);

// Worker thread signaling
pthread_mutex_lock(task->task_mutex);
task->completed = true;
pthread_cond_signal(task->task_cond);  // Wake up client thread
pthread_mutex_unlock(task->task_mutex);
```

---

## 🐛 Debugging Tips

### Server not starting?
```bash
# Check if port is already in use
netstat -tuln | grep 8080

# Kill existing server
killall server
```

### Client can't connect?
```bash
# Check server is running
ps aux | grep server

# Check firewall (if remote)
telnet localhost 8080
```

### Memory leaks?
```bash
# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./server

# In another terminal, run client and do operations
./client
# ... test ...

# Stop server with Ctrl+C
# Check Valgrind output for leaks
```

### Race conditions?
```bash
# Build with ThreadSanitizer
make tsan

# Run server
./server

# Run multiple clients simultaneously
./client &
./client &
./client &

# Check for warnings in server output
```

---

## 📊 Testing Checklist

### Manual Tests:
- [ ] Server starts without errors
- [ ] Client connects successfully
- [ ] Signup with new username works
- [ ] Login with correct credentials works
- [ ] Login with wrong credentials fails
- [ ] Upload file < 10MB works
- [ ] Upload file > 10MB fails (quota)
- [ ] LIST shows uploaded files
- [ ] DOWNLOAD retrieves correct file
- [ ] DELETE removes file
- [ ] Second upload same file updates it
- [ ] QUIT disconnects cleanly
- [ ] Ctrl+C shuts down server gracefully

### Automated Tests:
```bash
# Run test script
chmod +x test_phase1.sh
./test_phase1.sh
```

### Memory/Race Tests:
```bash
# Valgrind test
make clean
valgrind --leak-check=full ./server
# ... run client tests ...
# Ctrl+C server
# Check output

# ThreadSanitizer test
make tsan
./server
# ... run multiple clients ...
# Check for race warnings
```

---

## 🎯 Common Issues & Solutions

### Issue: "Address already in use"
**Solution:** Previous server still running or port not released
```bash
killall server
# Wait 30 seconds or use different port
```

### Issue: "Connection refused"
**Solution:** Server not started or firewall blocking
```bash
# Check server status
ps aux | grep server

# Test port
telnet localhost 8080
```

### Issue: "Segmentation fault"
**Solution:** Likely null pointer or memory corruption
```bash
# Run with debugger
gdb ./server
(gdb) run
# ... reproduce crash ...
(gdb) backtrace
```

### Issue: "File not found" on download
**Solution:** Check file was uploaded to correct user directory
```bash
ls -la ./storage/username/
```

---

## 📈 Performance Tips

### Current Configuration:
- 5 Client Threads
- 3 Worker Threads
- 10MB per-user quota

### Tuning (in server.h):
```c
#define CLIENT_THREADPOOL_SIZE 5   // Increase for more concurrent clients
#define WORKER_THREADPOOL_SIZE 3   // Increase for more concurrent I/O
#define USER_QUOTA 10485760        // Adjust quota size
```

---

## 🎓 Learning Objectives Achieved

After completing Phase 1, you should understand:

- ✅ Producer-consumer pattern with queues
- ✅ Thread pools and work distribution
- ✅ Mutex and condition variable usage
- ✅ Avoiding race conditions
- ✅ Preventing memory leaks
- ✅ Socket programming basics
- ✅ Multi-threaded server architecture
- ✅ Resource cleanup and graceful shutdown

---

## 📞 Need Help?

1. Check `server.log` for error messages
2. Use `gdb` for debugging crashes
3. Review `DESIGN.md` for architecture details
4. Check `README.md` for complete documentation

**Good luck with your project! 🚀**