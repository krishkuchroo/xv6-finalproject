// User-level threading library implementation for xv6
// This file contains the core threading functionality

// For xv6, we'll need to include appropriate headers
// In actual xv6 integration, these would be xv6 headers
// For now, using placeholder includes

// Global thread table and state
struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

// Forward declarations
static void thread_wrapper(void);
static struct thread* find_runnable_thread(void);

// ===== Part 1.1: Thread Initialization and Management =====

void thread_init(void) {
    // Initialize all thread slots to T_UNUSED
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].tid = 0;
        threads[i].state = T_UNUSED;
        threads[i].sp = 0;
        threads[i].start_routine = 0;
        threads[i].arg = 0;
        threads[i].retval = 0;
        threads[i].joined_tid = -1;
    }

    // Set up thread 0 as the main thread (already running)
    threads[0].tid = 0;
    threads[0].state = T_RUNNING;
    threads[0].joined_tid = -1;
    current_thread = &threads[0];
    next_tid = 1;
}

int thread_create(void* (*start_routine)(void*), void *arg) {
    // Find an unused thread slot
    struct thread *t = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (t == 0) {
        return -1;  // No available thread slots
    }

    // Initialize the thread structure
    t->tid = next_tid++;
    t->state = T_RUNNABLE;
    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = 0;
    t->joined_tid = -1;

    // Set up the stack
    // Stack grows downward, so sp starts at the top
    // We need to set up the stack so that when thread_switch
    // restores context, it will call thread_wrapper

    // Initialize stack pointer to top of stack
    char *sp = t->stack + STACK_SIZE;

    // Push thread_wrapper address (return address for thread_switch)
    sp -= sizeof(void*);
    *((void**)sp) = (void*)thread_wrapper;

    // Reserve space for saved registers (thread_switch will restore these)
    // x86 calling convention: we need to save ebp, ebx, esi, edi
    sp -= 4 * sizeof(void*);

    // Save the stack pointer
    t->sp = (void*)sp;

    return t->tid;
}

// Wrapper function that calls the thread's start_routine
// and then calls thread_exit with the return value
static void thread_wrapper(void) {
    // Get the current thread's start_routine and arg
    void *(*start_routine)(void*) = current_thread->start_routine;
    void *arg = current_thread->arg;

    // Call the thread's function
    void *retval = start_routine(arg);

    // Exit the thread with its return value
    thread_exit(retval);
}

void *thread_join(int tid) {
    // Find the thread with the given tid
    struct thread *t = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == tid && threads[i].state != T_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (t == 0) {
        return 0;  // Thread not found
    }

    // Wait for the thread to finish
    while (t->state != T_ZOMBIE) {
        // Mark this thread as waiting for the target thread
        current_thread->joined_tid = tid;

        // Block this thread
        current_thread->state = T_SLEEPING;

        // Run another thread
        thread_schedule();

        // When we wake up, check if thread is finished
    }

    // Collect the return value
    void *retval = t->retval;

    // Clean up the thread slot
    t->state = T_UNUSED;
    t->tid = 0;

    return retval;
}

void thread_exit(void *retval) {
    // Save the return value
    current_thread->retval = retval;

    // Mark this thread as finished
    current_thread->state = T_ZOMBIE;

    // Wake up any thread that is waiting for this thread
    int my_tid = current_thread->tid;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_SLEEPING && threads[i].joined_tid == my_tid) {
            threads[i].state = T_RUNNABLE;
            threads[i].joined_tid = -1;
        }
    }

    // Schedule another thread (this function never returns)
    thread_schedule();

    // Should never reach here
    while(1);
}

int thread_self(void) {
    return current_thread->tid;
}

void thread_yield(void) {
    // Mark current thread as runnable (not sleeping)
    current_thread->state = T_RUNNABLE;

    // Call scheduler to run another thread
    thread_schedule();
}

// ===== Part 1.3: Scheduler =====

void thread_schedule(void) {
    struct thread *old = current_thread;
    struct thread *next = find_runnable_thread();

    // If no runnable thread found, continue with current thread
    if (next == 0) {
        // If current thread is sleeping or zombie, we have a problem
        // In a real system, this would be a panic
        return;
    }

    // Update states
    if (old->state == T_RUNNING) {
        old->state = T_RUNNABLE;
    }
    // If old thread is T_SLEEPING or T_ZOMBIE, keep that state

    next->state = T_RUNNING;
    current_thread = next;

    // Perform context switch
    if (old != next) {
        thread_switch(old, next);
    }
}

// Helper function: find next runnable thread using round-robin
static struct thread* find_runnable_thread(void) {
    // Start searching from the thread after current_thread
    int start_idx = (current_thread - threads + 1) % MAX_THREADS;

    // First pass: search from start_idx to end of array
    for (int i = start_idx; i < MAX_THREADS; i++) {
        if (threads[i].state == T_RUNNABLE) {
            return &threads[i];
        }
    }

    // Second pass: wrap around from beginning to start_idx
    for (int i = 0; i < start_idx; i++) {
        if (threads[i].state == T_RUNNABLE) {
            return &threads[i];
        }
    }

    // If current thread is runnable, return it
    if (current_thread->state == T_RUNNABLE) {
        return current_thread;
    }

    // No runnable thread found
    return 0;
}

// ===== Part 2.1: Mutex Implementation =====

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner_tid = -1;
    m->wait_count = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        m->wait_queue[i] = -1;
    }
}

void mutex_lock(mutex_t *m) {
    // Try to acquire the lock
    while (m->locked) {
        // Lock is held by another thread, so block

        // Add current thread to wait queue
        m->wait_queue[m->wait_count++] = current_thread->tid;

        // Block this thread
        current_thread->state = T_SLEEPING;

        // Run another thread
        thread_schedule();

        // When we wake up, try again
    }

    // Acquire the lock
    m->locked = 1;
    m->owner_tid = current_thread->tid;
}

void mutex_unlock(mutex_t *m) {
    // Verify that current thread owns the lock
    if (m->owner_tid != current_thread->tid) {
        // Error: trying to unlock a mutex we don't own
        return;
    }

    // Check if any threads are waiting
    if (m->wait_count > 0) {
        // Wake up the first waiting thread
        int wake_tid = m->wait_queue[0];

        // Remove from wait queue (shift all elements)
        for (int i = 0; i < m->wait_count - 1; i++) {
            m->wait_queue[i] = m->wait_queue[i + 1];
        }
        m->wait_count--;

        // Find and wake the thread
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == wake_tid) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }

    // Release the lock
    m->locked = 0;
    m->owner_tid = -1;
}

// ===== Part 2.3: Semaphore Implementation =====

void sem_init(sem_t *s, int value) {
    s->count = value;
    s->wait_count = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        s->wait_queue[i] = -1;
    }
}

void sem_wait(sem_t *s) {
    // Decrement the count
    s->count--;

    // If count is negative, block
    while (s->count < 0) {
        // Add current thread to wait queue
        s->wait_queue[s->wait_count++] = current_thread->tid;

        // Block this thread
        current_thread->state = T_SLEEPING;

        // Run another thread
        thread_schedule();

        // When we wake up, we've been granted access
        break;
    }
}

void sem_post(sem_t *s) {
    // Increment the count
    s->count++;

    // If there were waiting threads (count was negative), wake one
    if (s->wait_count > 0) {
        // Wake up the first waiting thread
        int wake_tid = s->wait_queue[0];

        // Remove from wait queue
        for (int i = 0; i < s->wait_count - 1; i++) {
            s->wait_queue[i] = s->wait_queue[i + 1];
        }
        s->wait_count--;

        // Find and wake the thread
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == wake_tid) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}

// ===== Part 2.4: Condition Variable Implementation =====

void cond_init(cond_t *c) {
    c->wait_count = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        c->wait_queue[i] = -1;
    }
}

void cond_wait(cond_t *c, mutex_t *m) {
    // Add current thread to wait queue
    c->wait_queue[c->wait_count++] = current_thread->tid;

    // Release the mutex
    mutex_unlock(m);

    // Block this thread
    current_thread->state = T_SLEEPING;

    // Run another thread
    thread_schedule();

    // When we wake up, re-acquire the mutex
    mutex_lock(m);
}

void cond_signal(cond_t *c) {
    // If no threads are waiting, do nothing
    if (c->wait_count == 0) {
        return;
    }

    // Wake up the first waiting thread
    int wake_tid = c->wait_queue[0];

    // Remove from wait queue
    for (int i = 0; i < c->wait_count - 1; i++) {
        c->wait_queue[i] = c->wait_queue[i + 1];
    }
    c->wait_count--;

    // Find and wake the thread
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == wake_tid) {
            threads[i].state = T_RUNNABLE;
            break;
        }
    }
}

void cond_broadcast(cond_t *c) {
    // Wake up all waiting threads
    while (c->wait_count > 0) {
        int wake_tid = c->wait_queue[0];

        // Remove from wait queue
        for (int i = 0; i < c->wait_count - 1; i++) {
            c->wait_queue[i] = c->wait_queue[i + 1];
        }
        c->wait_count--;

        // Find and wake the thread
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == wake_tid) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}

// ===== Part 2.5: Channel Implementation =====

// Note: For xv6, we need malloc/free from umalloc.c
// This is a placeholder - in actual xv6, you'd use the xv6 malloc

channel_t* channel_create(int capacity) {
    // In xv6, use malloc from umalloc.c
    // For now, this is a placeholder
    channel_t *ch = 0; // malloc(sizeof(channel_t));
    if (ch == 0) {
        return 0;
    }

    ch->buffer = 0; // malloc(capacity * sizeof(void*));
    if (ch->buffer == 0) {
        // free(ch);
        return 0;
    }

    ch->capacity = capacity;
    ch->count = 0;
    ch->read_pos = 0;
    ch->write_pos = 0;
    ch->closed = 0;

    mutex_init(&ch->lock);
    cond_init(&ch->not_empty);
    cond_init(&ch->not_full);

    return ch;
}

int channel_send(channel_t *ch, void *data) {
    mutex_lock(&ch->lock);

    // Check if channel is closed
    if (ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    // Wait while buffer is full
    while (ch->count == ch->capacity) {
        cond_wait(&ch->not_full, &ch->lock);

        // Check again if channel was closed while waiting
        if (ch->closed) {
            mutex_unlock(&ch->lock);
            return -1;
        }
    }

    // Add data to buffer
    ch->buffer[ch->write_pos] = data;
    ch->write_pos = (ch->write_pos + 1) % ch->capacity;
    ch->count++;

    // Signal that data is available
    cond_signal(&ch->not_empty);

    mutex_unlock(&ch->lock);
    return 0;
}

int channel_recv(channel_t *ch, void **data) {
    mutex_lock(&ch->lock);

    // Wait while buffer is empty
    while (ch->count == 0) {
        // If channel is closed and empty, return error
        if (ch->closed) {
            mutex_unlock(&ch->lock);
            return -1;
        }

        cond_wait(&ch->not_empty, &ch->lock);
    }

    // Retrieve data from buffer
    *data = ch->buffer[ch->read_pos];
    ch->read_pos = (ch->read_pos + 1) % ch->capacity;
    ch->count--;

    // Signal that space is available
    cond_signal(&ch->not_full);

    mutex_unlock(&ch->lock);
    return 0;
}

void channel_close(channel_t *ch) {
    mutex_lock(&ch->lock);

    ch->closed = 1;

    // Wake up all waiting threads
    cond_broadcast(&ch->not_empty);
    cond_broadcast(&ch->not_full);

    mutex_unlock(&ch->lock);
}
