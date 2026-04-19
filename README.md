# Custom Heap Allocator

A lightweight, manual memory management library written in C.

## Features
- **Core Functions**: Implementation of `heap_malloc`, `heap_calloc`, `heap_realloc`, and `heap_free`.
- **Heap Integrity**: Uses checksums (`calculating_bytes`) to verify metadata consistency.
- **Corruption Detection**: Implements "fences" (guard bands) with specific symbols (`#`) to detect buffer overflows.
- **Pointer Validation**: A robust `get_pointer_type` function to identify valid, unallocated, or corrupted pointers.
- **System Integration**: Direct interaction with the OS via `sbrk` and `brk`.

## Security Measures
The allocator protects the heap from common errors:
1. **Metadata Corruption**: Any modification to the control blocks is caught by checksum validation.
2. **Buffer Overflows**: Writing past the allocated buffer corrupts the fence symbols, which is detected during `heap_validate`.

## Build & Test
Compile with:
```bash
gcc main.c alocs.c -o heap_test
./heap_test
