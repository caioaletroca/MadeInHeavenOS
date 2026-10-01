.section .text
.globl thread_entry

.code64

.type thread_entry, @function
thread_entry:
    # Entry point for a new thread.
    # This function should never return.
    call thread_start
1:
    hlt
    jmp 1b
.size thread_entry, .-thread_entry
