# Context switch for user-level threads
#
# void thread_switch(struct thread *old, struct thread *next);
#
# Save current thread's registers to its stack,
# save its stack pointer to its thread structure,
# load next thread's stack pointer from its thread structure,
# restore next thread's registers from its stack.
#
# This is based on xv6's swtch.S but adapted for user-level threads.

.globl thread_switch
thread_switch:
    # Function arguments (x86 calling convention):
    # 4(%esp) = old (pointer to current thread struct)
    # 8(%esp) = next (pointer to next thread struct)

    movl 4(%esp), %eax      # eax = old thread pointer
    movl 8(%esp), %edx      # edx = next thread pointer

    # Save old thread's registers
    # We need to save callee-saved registers: ebp, ebx, esi, edi
    # We also need to save the return address (current %eip)

    pushl %ebp              # Save base pointer
    pushl %ebx              # Save ebx
    pushl %esi              # Save esi
    pushl %edi              # Save edi

    # Save old thread's stack pointer
    # The 'sp' field is at offset 4+4+STACK_SIZE in struct thread
    # struct thread layout:
    #   int tid;           // offset 0
    #   int state;         // offset 4
    #   char stack[8192];  // offset 8
    #   void *sp;          // offset 8200 (8 + 8192)
    # Offset to sp field = 8 + STACK_SIZE = 8 + 8192 = 8200

    movl %esp, 8200(%eax)   # old->sp = esp

    # Load next thread's stack pointer
    movl 8200(%edx), %esp   # esp = next->sp

    # Restore next thread's registers
    popl %edi               # Restore edi
    popl %esi               # Restore esi
    popl %ebx               # Restore ebx
    popl %ebp               # Restore ebp

    # Return - this pops the return address and jumps to it
    # For a new thread, this will jump to thread_wrapper
    # For an existing thread, this returns to where it called thread_switch
    ret
