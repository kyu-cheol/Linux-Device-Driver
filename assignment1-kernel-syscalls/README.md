# Assignment 1 — Kernel Queue System Calls

Implements a small integer queue in the Linux kernel and exposes enqueue/dequeue operations through system calls. A userspace program invokes the calls by number.

## Files

- `linux/kernel/queue_syscalls.c` — queue storage and system-call implementations.
- `linux/include/uapi/asm-generic/unistd.h` — syscall numbers and declarations from the supplied exercise snapshot.
- `userspace/queue_app.c` — command-line enqueue/dequeue example.

## Integration

Apply the syscall source and registration changes to the matching kernel tree, update the relevant syscall build/table files as required by that kernel version, then rebuild and boot the kernel. The archive does not include every kernel integration file.
