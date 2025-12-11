# Quick Reference Guide - xv6 User-Level Threading Library

## API Quick Reference

### Thread Management

```c
// Initialize threading system (call first!)
void thread_init(void);

// Create a new thread
int thread_create(void* (*start_routine)(void*), void *arg);

// Wait for thread to finish, get return value
void *thread_join(int tid);

// Exit current thread with return value
void thread_exit(void *retval);

// Get current thread ID
int thread_self(void);

// Yield CPU to another thread
void thread_yield(void);
```

### Mutexes

```c
mutex_t lock;

// Initialize mutex
mutex_init(&lock);

// Acquire lock (blocks if held by another thread)
mutex_lock(&lock);

// Release lock (wakes one waiting thread)
mutex_unlock(&lock);
```

### Semaphores

```c
sem_t sem;

// Initialize semaphore with count
sem_init(&sem, initial_count);

// Decrement count, block if < 0
sem_wait(&sem);

// Increment count, wake one thread if waiting
sem_post(&sem);
```

### Condition Variables

```c
cond_t cond;
mutex_t mutex;

// Initialize condition variable
cond_init(&cond);

// Wait on condition (atomically unlocks mutex and sleeps)
cond_wait(&cond, &mutex);

// Wake one waiting thread
cond_signal(&cond);

// Wake all waiting threads
cond_broadcast(&cond);
```

### Channels

```c
channel_t *ch;

// Create channel with capacity
ch = channel_create(capacity);

// Send data (blocks if full)
channel_send(ch, data_pointer);

// Receive data (blocks if empty)
void *data;
channel_recv(ch, &data);

// Close channel (wakes all waiters)
channel_close(ch);
```

## Common Patterns

### Basic Threading

```c
void* my_thread(void *arg) {
    int id = *(int*)arg;
    printf("Thread %d running\n", id);
    return (void*)(long)id;
}

int main(void) {
    thread_init();

    int arg = 1;
    int tid = thread_create(my_thread, &arg);

    void *retval = thread_join(tid);
    printf("Thread returned: %d\n", (int)(long)retval);

    exit();
}
```

### Mutex Protected Critical Section

```c
mutex_t lock;
int shared_counter = 0;

void* increment(void *arg) {
    for (int i = 0; i < 1000; i++) {
        mutex_lock(&lock);
        shared_counter++;  // Critical section
        mutex_unlock(&lock);
    }
    return 0;
}

int main(void) {
    thread_init();
    mutex_init(&lock);

    // Create threads...
    // Join threads...
    // shared_counter will be correct!
}
```

### Producer-Consumer (Semaphores)

```c
#define BUFFER_SIZE 10
int buffer[BUFFER_SIZE];
sem_t empty, full;
mutex_t mutex;

void producer(void) {
    sem_wait(&empty);        // Wait for empty slot
    mutex_lock(&mutex);
    // Add to buffer
    mutex_unlock(&mutex);
    sem_post(&full);         // Signal full slot
}

void consumer(void) {
    sem_wait(&full);         // Wait for full slot
    mutex_lock(&mutex);
    // Remove from buffer
    mutex_unlock(&mutex);
    sem_post(&empty);        // Signal empty slot
}

int main(void) {
    thread_init();
    sem_init(&empty, BUFFER_SIZE);
    sem_init(&full, 0);
    mutex_init(&mutex);
    // Create producer and consumer threads...
}
```

### Condition Variable Wait Loop

```c
mutex_t mutex;
cond_t cond;
int ready = 0;

void* waiter(void *arg) {
    mutex_lock(&mutex);
    while (!ready) {
        cond_wait(&cond, &mutex);  // Atomically unlock and sleep
    }
    // Condition is now true, proceed...
    mutex_unlock(&mutex);
    return 0;
}

void* signaler(void *arg) {
    mutex_lock(&mutex);
    ready = 1;
    cond_signal(&cond);  // Wake one waiter
    mutex_unlock(&mutex);
    return 0;
}
```

### Producer-Consumer (Channels)

```c
channel_t *ch;

void* producer(void *arg) {
    for (int i = 0; i < 10; i++) {
        int *item = malloc(sizeof(int));
        *item = i;
        channel_send(ch, item);  // Blocks if full
    }
    return 0;
}

void* consumer(void *arg) {
    while (1) {
        void *data;
        int ret = channel_recv(ch, &data);
        if (ret == -1) break;  // Channel closed
        int item = *(int*)data;
        free(data);
        // Process item...
    }
    return 0;
}

int main(void) {
    thread_init();
    ch = channel_create(5);  // Buffer size 5

    // Create producers...
    // Wait for producers...

    channel_close(ch);  // Signal consumers to stop

    // Wait for consumers...
}
```

## Thread States Reference

```
T_UNUSED   = 0  // Thread slot available
T_RUNNABLE = 1  // Ready to run
T_RUNNING  = 2  // Currently executing
T_SLEEPING = 3  // Blocked (mutex, semaphore, cond, join)
T_ZOMBIE   = 4  // Finished, not yet joined
```

## Configuration Constants

```c
#define MAX_THREADS 16      // Maximum number of threads
#define STACK_SIZE 8192     // Stack size per thread (8KB)
```

## Common Mistakes to Avoid

### ❌ Forgetting to call thread_init()
```c
int main(void) {
    // DON'T: Create threads without init
    thread_create(my_func, NULL);  // WRONG!

    // DO: Initialize first
    thread_init();
    thread_create(my_func, NULL);  // Correct
}
```

### ❌ Not protecting shared data
```c
int counter = 0;  // Shared

void* thread(void *arg) {
    // DON'T: Race condition!
    counter++;  // WRONG!

    // DO: Use mutex
    mutex_lock(&lock);
    counter++;  // Correct
    mutex_unlock(&lock);
}
```

### ❌ Unlocking wrong mutex
```c
void* thread(void *arg) {
    mutex_lock(&lock1);

    // DON'T: Unlock different mutex
    mutex_unlock(&lock2);  // WRONG!

    // DO: Unlock same mutex
    mutex_unlock(&lock1);  // Correct
}
```

### ❌ Forgetting to join threads
```c
int main(void) {
    thread_init();
    int tid = thread_create(my_func, NULL);

    // DON'T: Exit without joining
    exit();  // Thread may not finish!

    // DO: Join before exit
    thread_join(tid);  // Correct
    exit();
}
```

### ❌ Using condition variable without mutex
```c
cond_t cond;

void* thread(void *arg) {
    // DON'T: Wait without holding mutex
    cond_wait(&cond, &mutex);  // WRONG! (mutex not locked)

    // DO: Lock before wait
    mutex_lock(&mutex);
    while (!condition) {
        cond_wait(&cond, &mutex);  // Correct
    }
    mutex_unlock(&mutex);
}
```

### ❌ Forgetting to re-check condition after cond_wait
```c
void* thread(void *arg) {
    mutex_lock(&mutex);

    // DON'T: Use if statement
    if (!ready) {
        cond_wait(&cond, &mutex);  // WRONG! (spurious wakeups)
    }

    // DO: Use while loop
    while (!ready) {
        cond_wait(&cond, &mutex);  // Correct
    }

    mutex_unlock(&mutex);
}
```

## Debugging Tips

### Print thread state
```c
printf("Thread %d: state=%d\n", thread_self(), current_thread->state);
```

### Add yield points for testing
```c
// Force context switches to expose race conditions
counter++;
thread_yield();  // Switch threads here
```

### Check return values
```c
int tid = thread_create(func, arg);
if (tid < 0) {
    printf("Failed to create thread\n");
}
```

### Verify mutex ownership
```c
if (mutex.owner_tid != thread_self()) {
    printf("ERROR: Unlocking mutex we don't own!\n");
}
```

## Performance Tips

### Minimize critical sections
```c
// DON'T: Hold lock longer than needed
mutex_lock(&lock);
int x = shared_data;
expensive_computation(x);  // Don't do this while holding lock!
mutex_unlock(&lock);

// DO: Only lock when accessing shared data
mutex_lock(&lock);
int x = shared_data;
mutex_unlock(&lock);
expensive_computation(x);  // Do this outside critical section
```

### Use condition variables instead of busy-waiting
```c
// DON'T: Busy-wait
while (!ready) {
    thread_yield();  // Wastes CPU
}

// DO: Use condition variable
mutex_lock(&mutex);
while (!ready) {
    cond_wait(&cond, &mutex);  // Efficient blocking
}
mutex_unlock(&mutex);
```

### Batch operations when possible
```c
// Instead of many small critical sections...
for (int i = 0; i < 1000; i++) {
    mutex_lock(&lock);
    array[i] = value;
    mutex_unlock(&lock);
}

// ...batch them:
mutex_lock(&lock);
for (int i = 0; i < 1000; i++) {
    array[i] = value;
}
mutex_unlock(&lock);
```

## Integration Checklist

- [ ] Files copied to xv6 directory
- [ ] Test files have `t_` prefix
- [ ] All files include xv6 headers (types.h, stat.h, user.h)
- [ ] Makefile has UTHREAD_LIB defined
- [ ] Makefile has _t_% rule
- [ ] UPROGS includes threaded programs
- [ ] `make clean && make` succeeds
- [ ] Programs run in xv6

## Testing Checklist

- [ ] Basic threading works (creation, joining)
- [ ] Context switching works (threads interleave)
- [ ] Mutex prevents race condition
- [ ] Semaphores block/wake correctly
- [ ] Condition variables signal/wait correctly
- [ ] Channels send/receive/close correctly
- [ ] Producer-Consumer runs without deadlock
- [ ] Reader-Writer maintains correctness

## Documentation Checklist

- [ ] Design document complete
- [ ] Architecture diagrams included
- [ ] Implementation explained
- [ ] Design decisions justified
- [ ] Test results documented
- [ ] Video demonstrates code and execution
- [ ] Video < 10 minutes
- [ ] Video has timestamps

## Submission Checklist

- [ ] GitHub PR created (project-branch → main)
- [ ] TAs added as collaborators
- [ ] DESIGN.md exported to PDF
- [ ] PDF uploaded to Brightspace
- [ ] YouTube video uploaded (unlisted)
- [ ] Video link submitted to Brightspace
- [ ] PR link submitted to Brightspace

---

## Quick Command Reference

### Build xv6 with threading library
```bash
cd xv6-public
make clean
make
```

### Run xv6
```bash
make qemu-nox  # Text mode
# or
make qemu      # Graphics mode
```

### In xv6 shell
```bash
$ ls                    # List programs
$ t_basic_thread_test   # Run basic test
$ t_mutex_test          # Run mutex test
$ t_producer_consumer_sem   # Run producer-consumer
```

### Debug
```bash
make qemu-nox-gdb       # Run with gdb support
# In another terminal:
gdb kernel
```

---

## Need Help?

1. Check `INTEGRATION_GUIDE.md` for integration steps
2. Check `DESIGN.md` for implementation details
3. Check `README.md` for overview
4. Review xv6 source code (proc.c, swtch.S)
5. Ask on class Slack
