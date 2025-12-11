# xv6 User-Level Threading Library

A complete user-level threading library for xv6, implementing cooperative multithreading and synchronization primitives entirely in user space.

## Project Structure

```
xv6-threading-project/
├── user_threading_library_core/
│   ├── src/                    # Core library implementation
│   │   ├── uthreads.h         # Public API interface
│   │   ├── uthreads.c         # Threading implementation
│   │   └── uthreads_swtch.S   # x86 context switching
│   ├── tests/                  # Test programs
│   │   └── mutex_test.c       # Shared counter test
│   └── examples/               # Part 3 concurrency problems
│       ├── producer_consumer_sem.c
│       ├── producer_consumer_chan.c
│       └── reader_writer.c
├── DESIGN.md                   # Design document
└── README.md                   # This file
```

## Components Implemented

### Part 1: Threading Foundation (60 points)

✅ **Thread States and Structure**
- Thread states: T_UNUSED, T_RUNNABLE, T_RUNNING, T_SLEEPING, T_ZOMBIE
- Thread structure with TID, state, stack, stack pointer, function, args, return value
- Global thread table (MAX_THREADS = 16)
- 8KB stack per thread

✅ **Core Threading API**
- `thread_init()` - Initialize threading system
- `thread_create()` - Create new thread
- `thread_join()` - Wait for thread completion
- `thread_exit()` - Terminate current thread
- `thread_self()` - Get current thread ID
- `thread_yield()` - Voluntarily yield CPU

✅ **Context Switching (x86 Assembly)**
- `thread_switch()` - Save/restore thread context
- Implements callee-saved register preservation
- Stack pointer manipulation for thread switching

✅ **Scheduler**
- Round-robin scheduling algorithm
- Cooperative multithreading (no preemption)
- `thread_schedule()` - Select next runnable thread

### Part 2: Synchronization Primitives (35 points)

✅ **Mutexes**
- `mutex_init()`, `mutex_lock()`, `mutex_unlock()`
- Wait queue for blocked threads
- Owner tracking for correct unlock

✅ **Semaphores**
- `sem_init()`, `sem_wait()`, `sem_post()`
- Counter-based synchronization
- Support for resource counting

✅ **Condition Variables**
- `cond_init()`, `cond_wait()`, `cond_signal()`, `cond_broadcast()`
- Works with mutexes for safe waiting
- Atomic unlock-and-sleep operation

✅ **Channels** (Extra Credit)
- `channel_create()`, `channel_send()`, `channel_recv()`, `channel_close()`
- Bounded buffer implementation
- Go-style message passing concurrency

### Part 3: Concurrency Problems (Extra Credit)

✅ **Producer-Consumer with Semaphores**
- 3 producers, 2 consumers
- Bounded buffer (size 5)
- Correct synchronization with semaphores

✅ **Producer-Consumer with Channels**
- Channel-based message passing
- Demonstrates "share by communicating" model

✅ **Reader-Writer Lock with Writer Priority**
- Multiple concurrent readers
- Exclusive writer access
- Writer priority to prevent starvation

## Key Design Decisions

### 1. User-Level vs Kernel-Level Threading

This library implements **N:1 user-level threading**:
- xv6 kernel is unaware of threads (only sees processes)
- All thread management happens in user space
- Cooperative scheduling (threads voluntarily yield)
- Lightweight context switches (no system calls)

**Advantages:**
- Fast thread creation and switching
- Flexible scheduling policies
- No kernel modifications needed

**Limitations:**
- Blocking system calls block all threads
- No true parallelism (single CPU core)
- Requires cooperative yielding

### 2. Context Switching Strategy

The assembly implementation (`uthreads_swtch.S`) follows xv6's kernel-level `swtch.S`:

```asm
thread_switch:
    # Save old thread's callee-saved registers
    pushl %ebp, %ebx, %esi, %edi

    # Save old thread's stack pointer
    movl %esp, 8200(%eax)  # old->sp = esp

    # Load new thread's stack pointer
    movl 8200(%edx), %esp  # esp = next->sp

    # Restore new thread's registers
    popl %edi, %esi, %ebx, %ebp

    ret  # Jump to new thread
```

**Critical offset calculation:**
```c
struct thread {
    int tid;           // offset 0
    int state;         // offset 4
    char stack[8192];  // offset 8
    void *sp;          // offset 8200 (8 + 8192)
    ...
};
```

### 3. Thread Stack Setup

New threads are initialized with a carefully crafted stack:

```
High Address (top of stack)
+------------------+
| (unused space)   |
+------------------+
| thread_wrapper   | <- Return address for first context switch
+------------------+
| saved %ebp       | <- Space for callee-saved registers
| saved %ebx       |
| saved %esi       |
| saved %edi       |
+------------------+ <- sp points here
| (growth space)   |
+------------------+
Low Address
```

When `thread_switch()` first restores this thread, it returns to `thread_wrapper()`, which:
1. Extracts `start_routine` and `arg` from thread structure
2. Calls `start_routine(arg)`
3. Calls `thread_exit(retval)` automatically

### 4. Scheduler Design

**Round-robin algorithm:**
```c
find_runnable_thread():
    start = (current_thread + 1) % MAX_THREADS
    search from start to MAX_THREADS
    wrap around from 0 to start
    return first T_RUNNABLE thread
```

**State transitions:**
- T_RUNNABLE → T_RUNNING: Selected by scheduler
- T_RUNNING → T_RUNNABLE: Calls thread_yield()
- T_RUNNING → T_SLEEPING: Blocks on mutex/semaphore/condition
- T_SLEEPING → T_RUNNABLE: Woken by unlock/signal
- T_RUNNING → T_ZOMBIE: Calls thread_exit()
- T_ZOMBIE → T_UNUSED: Joined and cleaned up

### 5. Synchronization Implementation

**Mutex:**
- Simple lock flag + owner TID
- Wait queue (array of TIDs)
- Blocking: Set state to T_SLEEPING, call scheduler

**Semaphore:**
- Integer counter (can be negative)
- Negative count = number of waiting threads
- `sem_wait()`: Decrement, block if negative
- `sem_post()`: Increment, wake if was negative

**Condition Variable:**
- Wait queue of sleeping threads
- `cond_wait()`: Release mutex, sleep, re-acquire on wake
- Critical: mutex unlock and sleep are atomic (no other thread runs between)

**Channel:**
- Circular buffer for bounded FIFO
- One mutex protects all state
- Two condition variables: `not_full`, `not_empty`
- `closed` flag for graceful shutdown

## Integration with xv6

### Makefile Modifications

To avoid filesystem size issues, use separate library linking:

```makefile
ULIB = ulib.o usys.o printf.o umalloc.o
UTHREAD_LIB = uthreads.o uthreads_swtch.o

# Regular programs
_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# Threaded programs (prefix: t_)
_t_%: %.o $(ULIB) $(UTHREAD_LIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

UPROGS=\
	_cat\
	_ls\
	_t_mutex_test\
	_t_producer_consumer_sem\
	_t_reader_writer
```

### Required xv6 Headers

Programs using this library need:
```c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"
```

### Using the Library

```c
int main(void) {
    // MUST call this first
    thread_init();

    // Create threads
    int tid = thread_create(my_function, arg);

    // Wait for completion
    void *result = thread_join(tid);

    exit();
}
```

## Testing

### Mutex Test
Demonstrates race condition without mutex and correct behavior with mutex protection.

**Run in xv6:**
```
$ t_mutex_test
```

**Expected output:**
- Without mutex: Incorrect counter value (race condition)
- With mutex: Correct counter value (3000)

### Producer-Consumer (Semaphores)
Tests bounded buffer with multiple producers and consumers.

**Run in xv6:**
```
$ t_producer_consumer_sem
```

### Reader-Writer Lock
Demonstrates writer priority preventing writer starvation.

**Run in xv6:**
```
$ t_reader_writer
```

## Known Limitations

1. **No malloc in xv6 user space for channels**: The channel implementation requires dynamic memory allocation. In actual xv6, you'd need to integrate with `umalloc.c`.

2. **No preemption**: Threads must cooperatively yield. A thread that doesn't yield will monopolize the CPU.

3. **Limited thread count**: MAX_THREADS = 16 to conserve memory.

4. **Blocking system calls**: If any thread makes a blocking syscall, ALL threads block.

## Future Enhancements

1. **Priority scheduling**: Add thread priorities
2. **Thread local storage**: Per-thread data storage
3. **Preemptive scheduling**: Use timer interrupts (requires kernel support)
4. **M:N threading**: Map N user threads to M kernel threads
5. **Thread pooling**: Reuse thread structures for efficiency

## References

- xv6 source code: `proc.c`, `swtch.S`
- POSIX threads (pthreads) specification
- Go programming language (goroutines and channels)
- "Operating Systems: Three Easy Pieces" by Remzi Arpaci-Dusseau

## Author

Krish Kuchroo
CS-GY 6233 - Introduction to Operating Systems
Fall 2025
