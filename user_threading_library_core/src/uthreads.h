#ifndef UTHREADS_H
#define UTHREADS_H

// Configuration Constants
#define MAX_THREADS 16
#define STACK_SIZE 8192  // 8KB per thread stack

// Thread States
#define T_UNUSED   0  // Thread slot is available
#define T_RUNNABLE 1  // Thread is ready to run
#define T_RUNNING  2  // Thread is currently executing
#define T_SLEEPING 3  // Thread is blocked (waiting on mutex/join)
#define T_ZOMBIE   4  // Thread has finished but not yet joined

// Thread Structure
struct thread {
    int tid;                    // Thread ID
    int state;                  // Thread state (T_UNUSED, T_RUNNABLE, etc.)
    char stack[STACK_SIZE];     // Thread's stack
    void *sp;                   // Saved stack pointer
    void *(*start_routine)(void*); // Starting function
    void *arg;                  // Argument to start_routine
    void *retval;               // Return value from thread
    int joined_tid;             // TID of thread waiting for this thread to finish
};

// Global thread table and current thread pointer
extern struct thread threads[MAX_THREADS];
extern struct thread *current_thread;
extern int next_tid;

// ===== Part 1: Threading Foundation API =====

// Initialize the threading system (must be called first)
void thread_init(void);

// Create a new thread
int thread_create(void* (*start_routine)(void*), void *arg);

// Wait for a thread to terminate and get its return value
void *thread_join(int tid);

// Terminate the currently running thread
void thread_exit(void *retval) __attribute__((noreturn));

// Get the TID of the currently running thread
int thread_self(void);

// Voluntarily yield the CPU to another thread
void thread_yield(void);

// Scheduler - selects next thread to run
void thread_schedule(void);

// Context switch (implemented in assembly)
void thread_switch(struct thread *old, struct thread *next);

// ===== Part 2: Synchronization Primitives =====

// Mutex structure
struct mutex {
    int locked;              // 0 = unlocked, 1 = locked
    int owner_tid;           // TID of thread holding the lock
    int wait_queue[MAX_THREADS]; // Queue of waiting thread TIDs
    int wait_count;          // Number of threads waiting
};

typedef struct mutex mutex_t;

// Mutex API
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

// Semaphore structure
struct semaphore {
    int count;               // Semaphore count
    int wait_queue[MAX_THREADS]; // Queue of waiting thread TIDs
    int wait_count;          // Number of threads waiting
};

typedef struct semaphore sem_t;

// Semaphore API
void sem_init(sem_t *s, int value);
void sem_wait(sem_t *s);
void sem_post(sem_t *s);

// Condition Variable structure
struct cond {
    int wait_queue[MAX_THREADS]; // Queue of waiting thread TIDs
    int wait_count;          // Number of threads waiting
};

typedef struct cond cond_t;

// Condition Variable API
void cond_init(cond_t *c);
void cond_wait(cond_t *c, mutex_t *m);
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);

// Channel structure (bounded buffer for message passing)
struct channel {
    void **buffer;           // Buffer to hold data pointers
    int capacity;            // Maximum buffer size
    int count;               // Current number of items in buffer
    int read_pos;            // Read position in circular buffer
    int write_pos;           // Write position in circular buffer
    int closed;              // 1 if channel is closed, 0 otherwise
    mutex_t lock;            // Protects channel state
    cond_t not_empty;        // Signaled when data is available
    cond_t not_full;         // Signaled when space is available
};

typedef struct channel channel_t;

// Channel API
channel_t* channel_create(int capacity);
int channel_send(channel_t *ch, void *data);
int channel_recv(channel_t *ch, void **data);
void channel_close(channel_t *ch);

#endif // UTHREADS_H
