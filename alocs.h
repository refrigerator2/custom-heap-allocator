#ifndef ALOC_HEAP_H
#define ALOC_HEAP_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define FENCE_SIZE 4
#define FENCE_SYM '#'
#define KB 4096

struct memory_manager_t
{
    void *memory_start;
    size_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size; 
    size_t used_size; 
    size_t size_of_all_block; 
    int free;
    size_t try_to_prevent_corrupt;
};

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

int heap_setup(void);
void heap_clean(void);

void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);

size_t   heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
int heap_validate(void);

#endif
