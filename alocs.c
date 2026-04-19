#include "alocs.h"

static struct memory_manager_t manager;

size_t calculating_bytes(void *memchunk) {
  if (!memchunk)
    return 0;
  struct memory_chunk_t *chunk = (struct memory_chunk_t *)memchunk;
  size_t len =
      (size_t)((uint8_t *)&chunk->try_to_prevent_corrupt - (uint8_t *)chunk);
  size_t sum = 0;
  uint8_t *ptr = (uint8_t *)chunk;
  for (size_t i = 0; i < len; i++) {
    sum += *(ptr + i);
  }
  return sum;
}

int heap_setup(void) {
  void *start = sbrk(0);
  if (start == (void *)-1)
    return -1;
  manager.memory_start = start;
  manager.memory_size = 0;
  manager.first_memory_chunk = NULL;
  return 0;
}

void heap_clean(void) {
  if (!manager.memory_start)
    return;
  brk(manager.memory_start);
  manager.memory_start = NULL;
  manager.memory_size = 0;
  manager.first_memory_chunk = NULL;
}

void *heap_malloc(size_t size) {
  if (!size)
    return NULL;
  struct memory_chunk_t *current = manager.first_memory_chunk;
  struct memory_chunk_t *prev = NULL;

  while (current) {
    if (current->free && current->size >= size) {
      current->free = 0;
      current->used_size = size;
      current->try_to_prevent_corrupt = calculating_bytes((void *)current);
      memset((uint8_t *)current + sizeof(struct memory_chunk_t) + FENCE_SIZE +
                 size,
             FENCE_SYM, FENCE_SIZE);
      return (uint8_t *)current + sizeof(struct memory_chunk_t) + FENCE_SIZE;
    }
    prev = current;
    current = current->next;
  }

  size_t size_of_all_block =
      sizeof(struct memory_chunk_t) + 2 * FENCE_SIZE + size;
  void *old_brk = sbrk(0);
  if (old_brk == (void *)-1)
    return NULL;
  if (sbrk((intptr_t)size_of_all_block) == (void *)-1)
    return NULL;

  memset(old_brk, 0, sizeof(struct memory_chunk_t));

  void *new_brk = sbrk(0);
  size_of_all_block = (size_t)((uint8_t *)new_brk - (uint8_t *)old_brk);

  struct memory_chunk_t *new_chunk = (struct memory_chunk_t *)old_brk;
  manager.memory_size = (uint8_t *)new_brk - (uint8_t *)manager.memory_start;

  new_chunk->prev = prev;
  new_chunk->next = NULL;
  new_chunk->free = 0;
  new_chunk->size = size;
  new_chunk->used_size = size;
  new_chunk->size_of_all_block = size_of_all_block;

  if (prev) {
    prev->next = new_chunk;
    prev->try_to_prevent_corrupt = calculating_bytes((void *)prev);
  } else {
    manager.first_memory_chunk = new_chunk;
  }

  memset((uint8_t *)new_chunk + sizeof(struct memory_chunk_t), FENCE_SYM,
         FENCE_SIZE);
  memset((uint8_t *)new_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE +
             size,
         FENCE_SYM, FENCE_SIZE);
  new_chunk->try_to_prevent_corrupt = calculating_bytes((void *)new_chunk);

  return (uint8_t *)new_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE;
}

void *heap_calloc(size_t number, size_t size) {
  if (!number || !size)
    return NULL;
  void *ptr = heap_malloc(number * size);
  if (!ptr)
    return NULL;
  memset(ptr, 0, number * size);
  return ptr;
}

void *heap_realloc(void *memblock, size_t count) {
  if (!manager.memory_start)
    return NULL;
  if (!memblock || get_pointer_type(memblock) == pointer_unallocated)
    return heap_malloc(count);
  if (get_pointer_type(memblock) != pointer_valid)
    return NULL;

  struct memory_chunk_t *chunk =
      (struct memory_chunk_t *)((uint8_t *)memblock -
                                sizeof(struct memory_chunk_t) - FENCE_SIZE);

  if (count <= chunk->size) {
    if (!count) {
      heap_free(memblock);
      return NULL;
    }
    chunk->used_size = count;
    memset((uint8_t *)chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE +
               count,
           FENCE_SYM, FENCE_SIZE);
    chunk->try_to_prevent_corrupt = calculating_bytes((void *)chunk);
    return memblock;
  }

  if (chunk->next && chunk->next->free &&
      (chunk->size + chunk->next->size_of_all_block >= count)) {
    size_t delta = count - chunk->size;
    struct memory_chunk_t *next = chunk->next;
    if (next->size_of_all_block - delta <=
        sizeof(struct memory_chunk_t) + 2 * FENCE_SIZE) {
      chunk->used_size = count;
      chunk->size += next->size_of_all_block;
      chunk->size_of_all_block += next->size_of_all_block;
      chunk->next = next->next;
      if (next->next) {
        next->next->prev = chunk;
        next->next->try_to_prevent_corrupt =
            calculating_bytes((void *)next->next);
      }
      memset((uint8_t *)chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE +
                 chunk->used_size,
             FENCE_SYM, FENCE_SIZE);
      chunk->try_to_prevent_corrupt = calculating_bytes((void *)chunk);
      return memblock;
    }
  }

  if (!chunk->next) {
    size_t needed = count - chunk->size;
    if (sbrk((intptr_t)needed) == (void *)-1)
      return NULL;
    void *new_brk = sbrk(0);

    size_t actual_added = (size_t)((uint8_t *)new_brk - (uint8_t *)chunk -
                                   chunk->size_of_all_block);
    chunk->size += actual_added;
    chunk->used_size = count;
    chunk->size_of_all_block += actual_added;
    manager.memory_size = (uint8_t *)new_brk - (uint8_t *)manager.memory_start;

    memset((uint8_t *)chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE +
               count,
           FENCE_SYM, FENCE_SIZE);
    chunk->try_to_prevent_corrupt = calculating_bytes((void *)chunk);
    return memblock;
  }

  uint8_t *new_ptr = heap_malloc(count);
  if (!new_ptr)
    return NULL;
  memcpy(new_ptr, memblock, chunk->used_size);
  heap_free(memblock);
  return new_ptr;
}

void heap_free(void *memblock) {
  if (memblock == NULL || get_pointer_type(memblock) != pointer_valid)
    return;
  struct memory_chunk_t *block =
      (struct memory_chunk_t *)((uint8_t *)memblock -
                                sizeof(struct memory_chunk_t) - FENCE_SIZE);
  block->free = 1;
  block->try_to_prevent_corrupt = calculating_bytes((void *)block);

  struct memory_chunk_t *cur = manager.first_memory_chunk;
  while (cur && cur->next != NULL) {
    if (cur->free && cur->next->free) {
      struct memory_chunk_t *next = cur->next;
      cur->size_of_all_block += next->size_of_all_block;
      cur->size += next->size_of_all_block;
      cur->next = next->next;
      if (next->next) {
        next->next->prev = cur;
        next->next->try_to_prevent_corrupt =
            calculating_bytes((void *)next->next);
      }
      cur->try_to_prevent_corrupt = calculating_bytes((void *)cur);
    } else {
      cur = cur->next;
    }
  }
}

size_t heap_get_largest_used_block_size(void) {
  if (!manager.memory_start || heap_validate() != 0)
    return 0;
  struct memory_chunk_t *current = manager.first_memory_chunk;
  size_t max_s = 0;
  while (current) {
    if (!current->free && current->used_size > max_s)
      max_s = current->used_size;
    current = current->next;
  }
  return max_s;
}

enum pointer_type_t get_pointer_type(const void *const pointer) {
  if (!pointer)
    return pointer_null;
  if (!manager.memory_start ||
      (uint8_t *)pointer < (uint8_t *)manager.memory_start ||
      (uint8_t *)pointer >=
          (uint8_t *)manager.memory_start + manager.memory_size)
    return pointer_unallocated;

  struct memory_chunk_t *current = manager.first_memory_chunk;
  while (current != NULL) {
    uint8_t *s_struct = (uint8_t *)current;
    uint8_t *s_fences = s_struct + sizeof(struct memory_chunk_t);
    uint8_t *user_data = s_fences + FENCE_SIZE;
    uint8_t *e_fences = user_data + current->used_size;

    if (current->try_to_prevent_corrupt != calculating_bytes(current))
      return pointer_heap_corrupted;

    if ((uint8_t *)pointer >= s_struct && (uint8_t *)pointer < s_fences)
      return current->free ? pointer_unallocated : pointer_control_block;
    if (((uint8_t *)pointer >= s_fences && (uint8_t *)pointer < user_data) ||
        ((uint8_t *)pointer >= e_fences &&
         (uint8_t *)pointer < e_fences + FENCE_SIZE))
      return current->free ? pointer_unallocated : pointer_inside_fences;
    if ((uint8_t *)pointer == user_data)
      return current->free ? pointer_unallocated : pointer_valid;
    if ((uint8_t *)pointer > user_data && (uint8_t *)pointer < e_fences)
      return current->free ? pointer_unallocated : pointer_inside_data_block;
    current = current->next;
  }
  return pointer_unallocated;
}

int heap_validate(void) {
  if (!manager.memory_start)
    return 2;
  struct memory_chunk_t *current = manager.first_memory_chunk;
  struct memory_chunk_t *prev = NULL;
  while (current != NULL) {
    if (current->try_to_prevent_corrupt != calculating_bytes(current))
      return 3;
    if (current->prev != prev)
      return 4;
    if (prev && prev->next != current)
      return 5;

    if (!current->free) {
      uint8_t *f_b = (uint8_t *)current + sizeof(struct memory_chunk_t);
      uint8_t *f_a = f_b + FENCE_SIZE + current->used_size;
      for (int i = 0; i < FENCE_SIZE; i++) {
        if (f_b[i] != FENCE_SYM || f_a[i] != FENCE_SYM)
          return 1;
      }
    }
    prev = current;
    current = current->next;
  }
  return 0;
}
