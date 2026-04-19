#include "alocs.h"

int main() {
    printf("--- Starting allocator tests ---\n");

    if (heap_setup() != 0) {
        printf("Heap setup failed\n");
        return 1;
    }

    void *ptr1 = heap_malloc(100);
    if (ptr1) printf("Allocated 100 bytes\n");

    void *ptr2 = heap_calloc(2, 50);
    if (ptr2) printf("Allocated 2x50 bytes via calloc\n");

    if (heap_validate() == 0) {
        printf("Heap is valid\n");
    }

    ptr1 = heap_realloc(ptr1, 200);
    if (ptr1) printf("Reallocated block to 200 bytes\n");

    if (get_pointer_type(ptr1) == pointer_valid) {
        printf("Pointer ptr1 is valid\n");
    }

    heap_free(ptr1);
    heap_free(ptr2);
    printf("Memory freed\n");

    heap_clean();
    printf("Heap cleaned. Tests finished successfully.\n");

    return 0;
}
