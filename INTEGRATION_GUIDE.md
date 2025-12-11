# Integration Guide: Adding Threading Library to xv6

This guide walks you through integrating the user-level threading library with your xv6 operating system.

## Prerequisites

- Working xv6 installation
- Basic understanding of Makefiles
- Familiarity with xv6's user program structure

## Step-by-Step Integration

### Step 1: Copy Library Files to xv6

```bash
# Navigate to your xv6 directory
cd /path/to/xv6-public

# Copy library source files
cp /path/to/user_threading_library_core/src/uthreads.h .
cp /path/to/user_threading_library_core/src/uthreads.c .
cp /path/to/user_threading_library_core/src/uthreads_swtch.S .

# Copy test files (rename with t_ prefix)
cp /path/to/user_threading_library_core/tests/basic_thread_test.c t_basic_thread_test.c
cp /path/to/user_threading_library_core/tests/mutex_test.c t_mutex_test.c

# Copy example files
cp /path/to/user_threading_library_core/examples/producer_consumer_sem.c t_producer_consumer_sem.c
cp /path/to/user_threading_library_core/examples/producer_consumer_chan.c t_producer_consumer_chan.c
cp /path/to/user_threading_library_core/examples/reader_writer.c t_reader_writer.c
```

### Step 2: Update Test Files to Include xv6 Headers

Each test file needs to include xv6 headers. Update the includes at the top of each `t_*.c` file:

**Before:**
```c
#include "../src/uthreads.h"
```

**After:**
```c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"
```

You can do this with sed:

```bash
# Update include statements
sed -i 's/#include "..\/src\/uthreads.h"/#include "types.h"\n#include "stat.h"\n#include "user.h"\n#include "uthreads.h"/' t_*.c
```

### Step 3: Fix uthreads.c for xv6

The `uthreads.c` file needs to include the proper header at the top:

```c
// Add at the top of uthreads.c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"
```

### Step 4: Update the Makefile

Open `Makefile` in your xv6 directory and make the following changes:

#### 4a. Define the threading library

Find the line with `ULIB =` and add below it:

```makefile
ULIB = ulib.o usys.o printf.o umalloc.o
UTHREAD_LIB = uthreads.o uthreads_swtch.o
```

#### 4b. Add build rule for threaded programs

Find the rule that looks like `_%: %.o $(ULIB)` and add this BEFORE it:

```makefile
# Threaded programs (prefix with t_)
_t_%: t_%.o $(ULIB) $(UTHREAD_LIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > t_$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > t_$*.sym
```

#### 4c. Add threading library build rules

Add these rules anywhere in the Makefile (good place is near other .o rules):

```makefile
uthreads.o: uthreads.c uthreads.h types.h stat.h user.h
	$(CC) $(CFLAGS) -c uthreads.c

uthreads_swtch.o: uthreads_swtch.S
	$(CC) $(ASFLAGS) -c uthreads_swtch.S
```

#### 4d. Add threaded programs to UPROGS

Find the `UPROGS=\` section and add your threaded programs:

```makefile
UPROGS=\
	_cat\
	_echo\
	_forktest\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_usertests\
	_wc\
	_zombie\
	_t_basic_thread_test\
	_t_mutex_test\
	_t_producer_consumer_sem\
	_t_reader_writer
```

Note: Only add `_t_producer_consumer_chan` if you've implemented dynamic memory allocation (malloc) support in the channel code.

#### 4e. Update clean target

Find the `clean:` target and add threading library objects:

```makefile
clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*.o *.d *.asm *.sym vectors.S bootblock entryother \
	initcode initcode.out kernel xv6.img fs.img kernelmemfs \
	xv6memfs.img mkfs .gdbinit \
	$(UPROGS) \
	uthreads.o uthreads_swtch.o
```

### Step 5: Fix Channels (Optional - Extra Credit)

If you're implementing channels, you need to fix the malloc/free calls:

In `uthreads.c`, update the `channel_create()` function:

```c
channel_t* channel_create(int capacity) {
    channel_t *ch = (channel_t*)malloc(sizeof(channel_t));
    if (ch == 0) {
        return 0;
    }

    ch->buffer = (void**)malloc(capacity * sizeof(void*));
    if (ch->buffer == 0) {
        free(ch);
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
```

The xv6 `umalloc.c` already provides `malloc()` and `free()`.

### Step 6: Handle Filesystem Size Issues

If you encounter "out of disk blocks" errors, you have three options:

#### Option A: Remove large unused programs (Quick fix)

Remove or comment out large programs you don't need from `UPROGS`:

```makefile
UPROGS=\
	_cat\
	_echo\
	# _forktest\    # Commented out
	_grep\
	# _stressfs\    # Commented out
	# _usertests\   # Commented out (very large)
	...
```

#### Option B: Increase NINDIRECT (Kernel modification)

Edit `fs.h` in xv6:

```c
// Before:
#define NINDIRECT (BSIZE / sizeof(uint))

// After:
#define NINDIRECT (BSIZE / sizeof(uint)) * 2  // or higher
```

**Warning:** This modifies kernel behavior. Document it in your design doc if you use it.

#### Option C: Use separate library linking (Recommended)

This is already done if you followed Step 4b! Only programs with `t_` prefix get the threading library.

### Step 7: Build xv6

```bash
make clean
make
```

If you see errors, check:
- All files are in the xv6 directory
- Makefile changes are correct
- Include statements are updated

### Step 8: Test the Library

Run xv6:

```bash
make qemu-nox
```

Or with graphics:

```bash
make qemu
```

In the xv6 shell, test your threading library:

```bash
$ t_basic_thread_test
Basic Threading Test
===================

Threading system initialized
Main thread TID: 0

Creating 3 threads...
Created thread 1 (TID: 1)
Created thread 2 (TID: 2)
Created thread 3 (TID: 3)
...
```

```bash
$ t_mutex_test
Shared Counter Test
===================

=== Test WITHOUT Mutex ===
...
RACE CONDITION DETECTED! Counter is incorrect.

=== Test WITH Mutex ===
...
SUCCESS! Counter is correct with mutex protection.
```

## Troubleshooting

### Error: "undefined reference to thread_switch"

**Problem:** Linker can't find the assembly function.

**Solution:** Make sure `uthreads_swtch.S` is compiled and linked:
- Check `UTHREAD_LIB` includes `uthreads_swtch.o`
- Check `.globl thread_switch` is in the `.S` file
- Make sure the `_t_%` rule includes `$(UTHREAD_LIB)`

### Error: "fs.img: ran out of blocks"

**Problem:** Filesystem image is too large.

**Solutions:**
1. Remove large programs from `UPROGS` (like `_usertests`, `_forktest`, `_stressfs`)
2. Reduce `STACK_SIZE` in `uthreads.h` (try 4096 instead of 8192)
3. Reduce `MAX_THREADS` in `uthreads.h` (try 8 instead of 16)
4. Use separate library linking (should already be done with `_t_` prefix)

### Error: "implicit declaration of function 'printf'"

**Problem:** Missing xv6 headers.

**Solution:** Add to top of file:
```c
#include "types.h"
#include "stat.h"
#include "user.h"
```

### Error: Threads not switching or hanging

**Problem:** Context switch assembly might have wrong offset.

**Solution:** Verify the `sp` offset in `uthreads_swtch.S`:

```c
// In uthreads.h:
struct thread {
    int tid;           // offset 0 (4 bytes)
    int state;         // offset 4 (4 bytes)
    char stack[8192];  // offset 8 (8192 bytes)
    void *sp;          // offset 8200 = 8 + 8192
    ...
};
```

The assembly should use offset 8200:
```asm
movl %esp, 8200(%eax)   # old->sp = esp
movl 8200(%edx), %esp   # esp = next->sp
```

### Error: "exit is not defined"

**Problem:** Test programs call `exit()` but it's not declared.

**Solution:** `exit()` is in `user.h`, make sure you include it.

## Verification Checklist

- [ ] All library files copied to xv6 directory
- [ ] Test files renamed with `t_` prefix
- [ ] All test files include xv6 headers (`types.h`, `stat.h`, `user.h`)
- [ ] `uthreads.c` includes xv6 headers
- [ ] Makefile updated with `UTHREAD_LIB`
- [ ] Makefile has rule for `_t_%` programs
- [ ] `UPROGS` includes threaded programs
- [ ] `make clean` succeeds
- [ ] `make` succeeds
- [ ] `make qemu` or `make qemu-nox` runs
- [ ] `t_basic_thread_test` runs and shows threads executing
- [ ] `t_mutex_test` shows race condition without mutex and correct behavior with mutex

## Next Steps

Once basic integration works:

1. Test all synchronization primitives
2. Run concurrency problem examples
3. Document any issues in design document
4. Create video demonstration
5. Prepare final submission

## Additional Resources

- xv6 book: [https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf](https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf)
- x86 calling conventions: [https://en.wikipedia.org/wiki/X86_calling_conventions](https://en.wikipedia.org/wiki/X86_calling_conventions)
- xv6 source code reference: Especially `proc.c` and `swtch.S`

Good luck with your implementation!
