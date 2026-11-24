# Assignment 2 — Ring Buffer Character Device

Implements a Linux character device backed by a 4096-byte ring buffer. File `write` enqueues bytes and file `read` dequeues them for userspace.

## Files

- `linux/drivers/ringbuffer/main.c` — module initialization, character-device file operations, ring-buffer state, and locking.

## Integration

Add the source to a compatible Linux kernel tree and provide the matching Kbuild entry before building the module. The archive contains no Makefile or userspace test program.
