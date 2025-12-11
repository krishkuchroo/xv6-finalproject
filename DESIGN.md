# xv6 User-Level Threading Library - Design Document

**Author:** Krish Kuchroo
**Course:** CS-GY 6233 - Introduction to Operating Systems
**Semester:** Fall 2025
**Project:** Final Project - User-Level Threading Library

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Thread Structure and Lifecycle](#thread-structure-and-lifecycle)
3. [Context Switching Mechanism](#context-switching-mechanism)
4. [Scheduler Design](#scheduler-design)
5. [Synchronization Primitives](#synchronization-primitives)
6. [Concurrency Problem Solutions](#concurrency-problem-solutions)
7. [Testing and Validation](#testing-and-validation)
8. [Design Decisions and Rationale](#design-decisions-and-rationale)

---

## 1. Architecture Overview

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    User Program                              │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │  Thread 1  │  │  Thread 2  │  │  Thread N  │            │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘            │
│        │                │                │                   │
│        └────────────────┴────────────────┘                   │
│                         │                                    │
│        ┌────────────────▼─────────────────┐                 │
│        │   User Threading Library (ULIB)  │                 │
│        │  ┌──────────────────────────┐    │                 │
│        │  │  Thread Management       │    │                 │
│        │  │  - thread_create()       │    │                 │
│        │  │  - thread_join()         │    │                 │
│        │  │  - thread_exit()         │    │                 │
│        │  └──────────────────────────┘    │                 │
│        │  ┌──────────────────────────┐    │                 │
│        │  │  Scheduler               │    │                 │
│        │  │  - Round-robin           │    │                 │
│        │  │  - Cooperative           │    │                 │
│        │  └──────────────────────────┘    │                 │
│        │  ┌──────────────────────────┐    │                 │
│        │  │  Context Switch (ASM)    │    │                 │
│        │  │  - Save registers        │    │                 │
│        │  │  - Switch stacks         │    │                 │
│        │  └──────────────────────────┘    │                 │
│        │  ┌──────────────────────────┐    │                 │
│        │  │  Synchronization         │    │                 │
│        │  │  - Mutexes               │    │                 │
│        │  │  - Semaphores            │    │                 │
│        │  │  - Condition Variables   │    │                 │
│        │  │  - Channels              │    │                 │
│        │  └──────────────────────────┘    │                 │
│        └──────────────────────────────────┘                 │
├─────────────────────────────────────────────────────────────┤
│                    xv6 Kernel                                │
│  (Thread-unaware - sees only a single process)              │
└─────────────────────────────────────────────────────────────┘
```

### N:1 Threading Model

This library implements **N:1 user-level threading**, where:
- **N user threads** map to **1 kernel process**
- The xv6 kernel schedules the process
- The user library schedules threads within the process

**Key Characteristics:**
- Cooperative multithreading (no preemption)
- Thread switches occur only at explicit yield points
- Extremely lightweight context switches (no system calls)
- All scheduling decisions made in user space

---

## 2. Thread Structure and Lifecycle

### Thread Structure Layout

```c
struct thread {
    int tid;                        // Thread ID (unique identifier)
    int state;                      // Current state (T_UNUSED, T_RUNNABLE, etc.)
    char stack[STACK_SIZE];         // 8KB dedicated stack
    void *sp;                       // Saved stack pointer
    void *(*start_routine)(void*);  // Entry point function
    void *arg;                      // Argument to start_routine
    void *retval;                   // Return value when thread exits
    int joined_tid;                 // TID of thread waiting to join this thread
};
```

**Memory Layout:**
```
Offset | Field          | Size  | Description
-------|----------------|-------|----------------------------------
0      | tid            | 4     | Thread identifier
4      | state          | 4     | Thread state constant
8      | stack          | 8192  | Thread's execution stack
8200   | sp             | 4     | Stack pointer (saved during switch)
8204   | start_routine  | 4     | Pointer to thread function
8208   | arg            | 4     | Argument pointer
8212   | retval         | 4     | Return value pointer
8216   | joined_tid     | 4     | Join synchronization
```

### Thread States

```
┌─────────────┐
│  T_UNUSED   │ ◄─── Initial state, thread slot available
└──────┬──────┘
       │ thread_create()
       ▼
┌─────────────┐
│ T_RUNNABLE  │ ◄─── Ready to run, waiting for CPU
└──────┬──────┘
       │ scheduler selects thread
       ▼
┌─────────────┐
│  T_RUNNING  │ ◄─── Currently executing on CPU
└──────┬──────┘
       │
       ├──── thread_yield() ────────┐
       │                            │
       ├──── mutex_lock() (blocked)─┤
       │                            ▼
       │                     ┌─────────────┐
       │                     │ T_SLEEPING  │ ◄─── Blocked, waiting
       │                     └──────┬──────┘
       │                            │ woken by unlock/signal
       │                            │
       │◄───────────────────────────┘
       │
       ├──── thread_exit() ─────────┐
       │                            ▼
       │                     ┌─────────────┐
       │                     │  T_ZOMBIE   │ ◄─── Finished, not yet joined
       │                     └──────┬──────┘
       │                            │ thread_join()
       │                            ▼
       └────────────────────► ┌─────────────┐
                              │  T_UNUSED   │ ◄─── Cleaned up, slot free
                              └─────────────┘
```

### Thread Lifecycle

1. **Creation** (`thread_create()`):
   - Find unused thread slot
   - Allocate TID
   - Initialize stack with `thread_wrapper` as return address
   - Set state to `T_RUNNABLE`

2. **First Execution**:
   - Scheduler picks thread
   - `thread_switch()` jumps to `thread_wrapper`
   - `thread_wrapper` calls `start_routine(arg)`

3. **Running**:
   - Thread executes normally
   - May yield voluntarily (`thread_yield()`)
   - May block on synchronization primitive

4. **Termination** (`thread_exit()`):
   - Save return value
   - Set state to `T_ZOMBIE`
   - Wake any joining thread
   - Call scheduler (never returns)

5. **Cleanup** (`thread_join()`):
   - Wait for thread to reach `T_ZOMBIE`
   - Collect return value
   - Set state to `T_UNUSED`

---

## 3. Context Switching Mechanism

### Overview

Context switching is the core mechanism that enables multithreading. It involves:
1. Saving the current thread's CPU state
2. Restoring the next thread's CPU state
3. Jumping to the next thread's execution point

### Assembly Implementation

**File:** `uthreads_swtch.S`

```asm
# void thread_switch(struct thread *old, struct thread *next)
#
# Arguments:
#   4(%esp) = old thread pointer
#   8(%esp) = next thread pointer

.globl thread_switch
thread_switch:
    movl 4(%esp), %eax      # eax = old thread
    movl 8(%esp), %edx      # edx = next thread

    # Save old thread's callee-saved registers
    pushl %ebp              # Save frame pointer
    pushl %ebx              # Save ebx
    pushl %esi              # Save source index
    pushl %edi              # Save destination index

    # Save old thread's stack pointer
    movl %esp, 8200(%eax)   # old->sp = esp

    # Load next thread's stack pointer
    movl 8200(%edx), %esp   # esp = next->sp

    # Restore next thread's registers
    popl %edi               # Restore edi
    popl %esi               # Restore esi
    popl %ebx               # Restore ebx
    popl %ebp               # Restore ebp

    # Return to next thread's execution point
    ret
```

### Why Assembly?

**Reason 1: Stack Pointer Manipulation**
- C functions operate on a single stack
- Context switching requires changing `%esp` (stack pointer)
- This is impossible to express in C

**Reason 2: Register Control**
- Need precise control over which registers to save/restore
- x86 calling convention distinguishes:
  - **Callee-saved:** `%ebp`, `%ebx`, `%esi`, `%edi` (we save these)
  - **Caller-saved:** `%eax`, `%ecx`, `%edx` (caller saves if needed)

**Reason 3: Return Address Manipulation**
- `ret` instruction pops return address from stack and jumps
- For new threads, return address is `thread_wrapper`
- For existing threads, return address is where they called `thread_switch`

### Stack Layout During Context Switch

**Before `thread_switch(old, next)`:**

```
Old Thread Stack              New Thread Stack
┌──────────────┐             ┌──────────────┐
│  ...         │             │  ...         │
│  return addr │ ◄── %ebp    │  return addr │
│  saved %ebp  │             │  saved %ebp  │
├──────────────┤             ├──────────────┤
│  old ptr     │ ◄── 4(%esp) │  (unused)    │
│  next ptr    │     8(%esp) │  (unused)    │
│  ret addr    │ ◄── %esp    │  (unused)    │
└──────────────┘             └──────────────┘
```

**After saving old registers:**

```
Old Thread Stack
┌──────────────┐
│  ...         │
│  ret addr    │
│  old ptr     │
│  next ptr    │
│  ret addr    │
├──────────────┤
│  saved %ebp  │ ◄─┐
│  saved %ebx  │   │ Pushed by
│  saved %esi  │   │ thread_switch
│  saved %edi  │ ◄─┘
└──────────────┘
    ▲
    │
   %esp (old->sp saved here)
```

**After loading new %esp and restoring registers:**

```
New Thread Stack
┌──────────────┐
│  ...         │
│  ret addr    │ ◄── Will return here
├──────────────┤
│  saved %ebp  │ ◄─┐ Already popped,
│  saved %ebx  │   │ registers
│  saved %esi  │   │ restored
│  saved %edi  │ ◄─┘
└──────────────┘
```

### New Thread Initialization

When creating a new thread, we set up its stack to look like it just called `thread_switch()`:

```c
// In thread_create():
char *sp = t->stack + STACK_SIZE;  // Start at top

// Push return address (thread_wrapper)
sp -= sizeof(void*);
*((void**)sp) = (void*)thread_wrapper;

// Reserve space for saved registers
sp -= 4 * sizeof(void*);  // ebp, ebx, esi, edi

t->sp = (void*)sp;
```

**Initial Stack Layout:**

```
High Address (stack top)
┌──────────────────┐
│   (unused)       │
├──────────────────┤
│ thread_wrapper   │ ◄── Return address for first switch
├──────────────────┤
│   (space for     │
│    saved %ebp)   │
│   (space for     │
│    saved %ebx)   │
│   (space for     │
│    saved %esi)   │
│   (space for     │
│    saved %edi)   │ ◄── sp points here
├──────────────────┤
│   (growth area)  │
└──────────────────┘
Low Address
```

When the scheduler first switches to this thread:
1. `thread_switch()` loads `sp` into `%esp`
2. Pops saved registers (restoring garbage, but that's OK)
3. Executes `ret`, which pops `thread_wrapper` and jumps to it

### Thread Wrapper Function

```c
static void thread_wrapper(void) {
    // Extract function and argument from current thread
    void *(*start_routine)(void*) = current_thread->start_routine;
    void *arg = current_thread->arg;

    // Call the thread's actual function
    void *retval = start_routine(arg);

    // Automatically exit when function returns
    thread_exit(retval);
}
```

**Purpose:**
- Provides a clean entry point for new threads
- Automatically calls `thread_exit()` when function returns
- Ensures threads can't "fall off the end" of their function

---

## 4. Scheduler Design

### Scheduling Algorithm: Round-Robin

**Properties:**
- **Fair:** Each thread gets equal CPU time
- **Simple:** Easy to understand and implement
- **Cooperative:** Threads must yield voluntarily

### Implementation

```c
void thread_schedule(void) {
    struct thread *old = current_thread;
    struct thread *next = find_runnable_thread();

    if (next == 0) {
        return;  // No runnable thread, keep current
    }

    // Update states
    if (old->state == T_RUNNING) {
        old->state = T_RUNNABLE;
    }

    next->state = T_RUNNING;
    current_thread = next;

    // Context switch
    if (old != next) {
        thread_switch(old, next);
    }
}
```

### Finding Next Runnable Thread

```c
static struct thread* find_runnable_thread(void) {
    // Start from thread after current
    int start = (current_thread - threads + 1) % MAX_THREADS;

    // Search from start to end
    for (int i = start; i < MAX_THREADS; i++) {
        if (threads[i].state == T_RUNNABLE) {
            return &threads[i];
        }
    }

    // Wrap around: search from 0 to start
    for (int i = 0; i < start; i++) {
        if (threads[i].state == T_RUNNABLE) {
            return &threads[i];
        }
    }

    // If current is runnable, return it
    if (current_thread->state == T_RUNNABLE) {
        return current_thread;
    }

    return 0;  // No runnable thread found
}
```

**Search Pattern Example:**

```
Thread Table:  [0] [1] [2] [3] [4] [5] [6] [7]
States:         R   R   S   Z   U   R   R   U
                    ▲
                    current_thread = 1

Search order:  2 → 3 → 4 → 5 → 6 → 7 → 0 → 1
                            ✓   ✓

Next thread selected: 5 (first runnable after current)
```

### Scheduling Points

Threads are scheduled when:

1. **Voluntary Yield** (`thread_yield()`):
   ```c
   void thread_yield(void) {
       current_thread->state = T_RUNNABLE;
       thread_schedule();
   }
   ```

2. **Blocking** (mutex, semaphore, condition variable):
   ```c
   void mutex_lock(mutex_t *m) {
       while (m->locked) {
           current_thread->state = T_SLEEPING;
           thread_schedule();  // Blocks here
       }
       m->locked = 1;
   }
   ```

3. **Thread Termination** (`thread_exit()`):
   ```c
   void thread_exit(void *retval) {
       current_thread->state = T_ZOMBIE;
       thread_schedule();  // Never returns
   }
   ```

4. **Waiting for Join** (`thread_join()`):
   ```c
   void *thread_join(int tid) {
       while (target->state != T_ZOMBIE) {
           current_thread->state = T_SLEEPING;
           thread_schedule();  // Blocks here
       }
       return target->retval;
   }
   ```

---

## 5. Synchronization Primitives

### 5.1 Mutexes (Mutual Exclusion)

**Purpose:** Ensure only one thread executes in a critical section.

**Structure:**
```c
struct mutex {
    int locked;                  // 0 = unlocked, 1 = locked
    int owner_tid;               // TID of lock owner
    int wait_queue[MAX_THREADS]; // Blocked threads
    int wait_count;              // Number waiting
};
```

**Implementation:**

```c
void mutex_lock(mutex_t *m) {
    while (m->locked) {
        // Add to wait queue
        m->wait_queue[m->wait_count++] = current_thread->tid;

        // Block
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }

    // Acquire lock
    m->locked = 1;
    m->owner_tid = current_thread->tid;
}

void mutex_unlock(mutex_t *m) {
    // Verify ownership
    if (m->owner_tid != current_thread->tid) {
        return;  // Error
    }

    // Wake one waiting thread
    if (m->wait_count > 0) {
        int wake_tid = m->wait_queue[0];

        // Remove from queue (shift array)
        for (int i = 0; i < m->wait_count - 1; i++) {
            m->wait_queue[i] = m->wait_queue[i + 1];
        }
        m->wait_count--;

        // Wake the thread
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == wake_tid) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }

    // Release lock
    m->locked = 0;
    m->owner_tid = -1;
}
```

**Key Design Decision:**
- **Why not spin-wait?** Spinning wastes CPU in a cooperative system. Instead, we block (set state to T_SLEEPING) and let other threads run.

---

### 5.2 Semaphores

**Purpose:** General counting synchronization primitive.

**Structure:**
```c
struct semaphore {
    int count;                   // Semaphore value
    int wait_queue[MAX_THREADS]; // Blocked threads
    int wait_count;
};
```

**Implementation:**

```c
void sem_wait(sem_t *s) {
    s->count--;

    if (s->count < 0) {
        // Add to wait queue
        s->wait_queue[s->wait_count++] = current_thread->tid;

        // Block
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }
}

void sem_post(sem_t *s) {
    s->count++;

    // Wake one waiting thread if count was negative
    if (s->wait_count > 0) {
        int wake_tid = s->wait_queue[0];

        // Remove from queue
        for (int i = 0; i < s->wait_count - 1; i++) {
            s->wait_queue[i] = s->wait_queue[i + 1];
        }
        s->wait_count--;

        // Wake thread
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == wake_tid) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}
```

**Semaphore Counting:**
- `count > 0`: Resources available
- `count == 0`: No resources, but no one waiting
- `count < 0`: `|count|` threads are waiting

---

### 5.3 Condition Variables

**Purpose:** Wait for a condition to become true, with atomic unlock-and-sleep.

**Structure:**
```c
struct cond {
    int wait_queue[MAX_THREADS];
    int wait_count;
};
```

**Implementation:**

```c
void cond_wait(cond_t *c, mutex_t *m) {
    // Add to wait queue
    c->wait_queue[c->wait_count++] = current_thread->tid;

    // CRITICAL: Unlock mutex and sleep atomically
    mutex_unlock(m);
    current_thread->state = T_SLEEPING;
    thread_schedule();

    // Re-acquire mutex when woken
    mutex_lock(m);
}

void cond_signal(cond_t *c) {
    if (c->wait_count == 0) {
        return;
    }

    int wake_tid = c->wait_queue[0];

    // Remove from queue
    for (int i = 0; i < c->wait_count - 1; i++) {
        c->wait_queue[i] = c->wait_queue[i + 1];
    }
    c->wait_count--;

    // Wake thread
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == wake_tid) {
            threads[i].state = T_RUNNABLE;
            break;
        }
    }
}

void cond_broadcast(cond_t *c) {
    // Wake all waiting threads
    while (c->wait_count > 0) {
        cond_signal(c);
    }
}
```

**Key Design Decision:**
- **Atomic unlock-and-sleep:** In our cooperative model, `mutex_unlock()` followed by `thread_schedule()` is atomic because no other thread can run between them (no preemption).

---

### 5.4 Channels (Message Passing)

**Purpose:** Go-style bounded buffer for thread communication.

**Structure:**
```c
struct channel {
    void **buffer;       // Circular buffer
    int capacity;        // Buffer size
    int count;           // Current items
    int read_pos;        // Read index
    int write_pos;       // Write index
    int closed;          // Closed flag
    mutex_t lock;        // Protects state
    cond_t not_empty;    // Signals data available
    cond_t not_full;     // Signals space available
};
```

**Implementation:**

```c
int channel_send(channel_t *ch, void *data) {
    mutex_lock(&ch->lock);

    // Wait while full
    while (ch->count == ch->capacity && !ch->closed) {
        cond_wait(&ch->not_full, &ch->lock);
    }

    if (ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    // Add to buffer
    ch->buffer[ch->write_pos] = data;
    ch->write_pos = (ch->write_pos + 1) % ch->capacity;
    ch->count++;

    // Signal receivers
    cond_signal(&ch->not_empty);

    mutex_unlock(&ch->lock);
    return 0;
}

int channel_recv(channel_t *ch, void **data) {
    mutex_lock(&ch->lock);

    // Wait while empty
    while (ch->count == 0) {
        if (ch->closed) {
            mutex_unlock(&ch->lock);
            return -1;
        }
        cond_wait(&ch->not_empty, &ch->lock);
    }

    // Remove from buffer
    *data = ch->buffer[ch->read_pos];
    ch->read_pos = (ch->read_pos + 1) % ch->capacity;
    ch->count--;

    // Signal senders
    cond_signal(&ch->not_full);

    mutex_unlock(&ch->lock);
    return 0;
}
```

**Design Pattern:**
```
Monitor Pattern:
1. Acquire lock
2. While (condition not met):
      Wait on condition variable
3. Perform operation
4. Signal other condition variable
5. Release lock
```

---

## 6. Concurrency Problem Solutions

### 6.1 Producer-Consumer (Semaphores)

**Problem:** Multiple producers generate items, multiple consumers process them, using a bounded buffer.

**Solution:**

```c
sem_t empty;  // Counts empty slots (init: BUFFER_SIZE)
sem_t full;   // Counts full slots (init: 0)
mutex_t buffer_mutex;

Producer:
    sem_wait(&empty);       // Wait for empty slot
    mutex_lock(&buffer_mutex);
    // Add item to buffer
    mutex_unlock(&buffer_mutex);
    sem_post(&full);        // Signal item added

Consumer:
    sem_wait(&full);        // Wait for full slot
    mutex_lock(&buffer_mutex);
    // Remove item from buffer
    mutex_unlock(&buffer_mutex);
    sem_post(&empty);       // Signal slot freed
```

**Why This Works:**
- `empty` semaphore prevents producers from overfilling
- `full` semaphore prevents consumers from reading empty buffer
- Mutex protects buffer structure from concurrent access

---

### 6.2 Producer-Consumer (Channels)

**Problem:** Same as above, but using channels for message passing.

**Solution:**

```c
channel_t *ch = channel_create(BUFFER_SIZE);

Producer:
    channel_send(ch, item);  // Blocks if full

Consumer:
    channel_recv(ch, &item); // Blocks if empty

Main:
    // After all producers finish
    channel_close(ch);       // Signals consumers to stop
```

**Why This Works:**
- Channel internally manages all synchronization
- Bounded buffer prevents memory overflow
- `channel_close()` provides clean shutdown mechanism

---

### 6.3 Reader-Writer Lock (Writer Priority)

**Problem:** Multiple readers can read simultaneously, but writers need exclusive access. Writers should not starve.

**Solution:**

```c
struct rwlock {
    int readers_active;   // Readers currently reading
    int writers_waiting;  // Writers waiting
    int writer_active;    // Writer currently writing
    mutex_t lock;
    cond_t readers_ok;
    cond_t writers_ok;
};

Reader:
    lock(&rw->lock);
    while (rw->writer_active || rw->writers_waiting > 0)
        wait(&rw->readers_ok, &rw->lock);
    rw->readers_active++;
    unlock(&rw->lock);

    // READ DATA

    lock(&rw->lock);
    rw->readers_active--;
    if (rw->readers_active == 0 && rw->writers_waiting > 0)
        signal(&rw->writers_ok);
    unlock(&rw->lock);

Writer:
    lock(&rw->lock);
    rw->writers_waiting++;
    while (rw->readers_active > 0 || rw->writer_active)
        wait(&rw->writers_ok, &rw->lock);
    rw->writers_waiting--;
    rw->writer_active = 1;
    unlock(&rw->lock);

    // WRITE DATA

    lock(&rw->lock);
    rw->writer_active = 0;
    if (rw->writers_waiting > 0)
        signal(&rw->writers_ok);  // Prioritize writers
    else
        broadcast(&rw->readers_ok);
    unlock(&rw->lock);
```

**Why Writer Priority Works:**
1. Readers check `writers_waiting > 0` before entering
2. If writers are waiting, new readers block
3. Last reader wakes a waiting writer
4. Finishing writer wakes another writer (if any), else wakes readers

---

## 7. Testing and Validation

### 7.1 Mutex Test (Shared Counter)

**Test Design:**
```c
Test 1: WITHOUT mutex
  - 3 threads increment counter 1000 times each
  - Expected: Race conditions cause lost updates
  - Result: Counter < 3000 (demonstrates the problem)

Test 2: WITH mutex
  - Same test, but with mutex protection
  - Expected: All updates counted correctly
  - Result: Counter == 3000 (demonstrates the solution)
```

**Example Output:**
```
=== Test WITHOUT Mutex ===
Created thread 0 (TID: 1)
Created thread 1 (TID: 2)
Created thread 2 (TID: 3)
Thread 1 completed
Thread 2 completed
Thread 3 completed
Expected counter value: 3000
Actual counter value: 2847
RACE CONDITION DETECTED! Counter is incorrect.

=== Test WITH Mutex ===
Created thread 0 (TID: 4)
Created thread 1 (TID: 5)
Created thread 2 (TID: 6)
Thread 4 completed
Thread 5 completed
Thread 6 completed
Expected counter value: 3000
Actual counter value: 3000
SUCCESS! Counter is correct with mutex protection.
```

---

## 8. Design Decisions and Rationale

### Decision 1: Cooperative vs Preemptive Scheduling

**Choice:** Cooperative scheduling

**Rationale:**
- xv6 user programs cannot handle timer interrupts
- No kernel support for preemption
- Simpler to implement and debug
- Sufficient for demonstrating threading concepts

**Trade-off:**
- ✅ Simpler implementation
- ✅ No race conditions within thread code
- ❌ Threads must yield voluntarily
- ❌ One misbehaving thread can monopolize CPU

---

### Decision 2: Round-Robin Scheduling

**Choice:** Simple round-robin algorithm

**Rationale:**
- Fair allocation of CPU time
- Easy to understand and verify
- No starvation (every thread eventually runs)

**Alternative Considered:**
- Priority scheduling (rejected: adds complexity)
- Shortest-job-first (rejected: unpredictable in general-purpose threading)

---

### Decision 3: Stack Size (8KB)

**Choice:** `STACK_SIZE = 8192` bytes

**Rationale:**
- Same as xv6 kernel stack size
- Sufficient for typical function call depth
- Balance between memory usage and safety

**Calculation:**
```
MAX_THREADS = 16
Total stack memory = 16 × 8KB = 128KB
xv6 process memory limit ≈ 640KB
Stack usage ≈ 20% of process memory (acceptable)
```

---

### Decision 4: Wait Queue Implementation

**Choice:** Simple array-based FIFO queue

**Rationale:**
- No dynamic allocation needed
- Fixed size (MAX_THREADS)
- Simple to implement and debug

**Alternative Considered:**
- Linked list (rejected: requires pointers in thread structure)
- Bitmap (rejected: doesn't preserve FIFO order)

---

### Decision 5: Atomic Unlock-and-Sleep

**Choice:** Rely on cooperative scheduling for atomicity

**Implementation:**
```c
void cond_wait(cond_t *c, mutex_t *m) {
    // Add to wait queue
    c->wait_queue[c->wait_count++] = current_thread->tid;

    // These operations are atomic because we're cooperative
    mutex_unlock(m);                // 1. Release mutex
    current_thread->state = T_SLEEPING;  // 2. Mark as sleeping
    thread_schedule();              // 3. Switch threads
    // No other thread can run between these steps!

    mutex_lock(m);  // Re-acquire when woken
}
```

**Rationale:**
- In preemptive systems, this would require disabling interrupts
- In our cooperative system, no other thread runs until we call `thread_schedule()`
- This makes the unlock-sleep operation naturally atomic

---

## Conclusion

This user-level threading library demonstrates the fundamental concepts of:
- Thread management and lifecycle
- Context switching at the machine level
- Cooperative scheduling algorithms
- Synchronization primitives for safe concurrent programming

The implementation follows classic OS design patterns while adapting them to the constraints of xv6's user-space environment.

---

## Appendices

### Appendix A: Testing Checklist

- [ ] Basic threading
  - [ ] `thread_create()` creates threads
  - [ ] `thread_join()` waits for completion
  - [ ] Threads execute and return values
  - [ ] Multiple threads interleave correctly

- [ ] Scheduling
  - [ ] Round-robin fairness
  - [ ] `thread_yield()` switches threads
  - [ ] Sleeping threads don't run
  - [ ] Zombie threads don't run

- [ ] Mutexes
  - [ ] Lock prevents concurrent access
  - [ ] Unlock wakes waiting thread
  - [ ] Owner verification works

- [ ] Semaphores
  - [ ] Count tracks resources correctly
  - [ ] Blocking when count < 0
  - [ ] Waking when count increases

- [ ] Condition Variables
  - [ ] `cond_wait()` releases lock and sleeps
  - [ ] `cond_signal()` wakes one thread
  - [ ] `cond_broadcast()` wakes all threads

- [ ] Channels
  - [ ] Send blocks when full
  - [ ] Recv blocks when empty
  - [ ] Close wakes all waiters

### Appendix B: Known Limitations

1. **No dynamic memory for channels:** Requires integration with `umalloc.c`
2. **Fixed thread limit:** MAX_THREADS = 16
3. **No thread priorities:** All threads equal
4. **No thread cancellation:** Threads must exit voluntarily
5. **No thread-local storage:** All variables are process-wide
6. **Blocking syscalls block all threads:** xv6 kernel doesn't know about threads

### Appendix C: Future Enhancements

1. **Preemptive scheduling** (requires kernel modification)
2. **Thread priorities**
3. **Thread pools** for efficiency
4. **Futexes** for faster synchronization
5. **M:N threading** (hybrid model)
6. **Better deadlock detection**
