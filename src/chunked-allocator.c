// Chunked allocator with O(1) average alloc and free time.
// Worse case alloc/free time matches that of the underlying malloc/free.
// Can only allocate a single fixed size.
//
// Due to a bug in MacOS's libc, this falls back to malloc/free on MacOS.

#ifdef TYPE

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef TYPED
#define TYPED(THING) THING
#endif

#define CHUNK_SIZE 65536 // Must be a power of 2
#define CHUNK_MASK (~CHUNK_SIZE + 1)
#define CHUNK_CAPACITY (CHUNK_SIZE / sizeof(TYPE))

#define Allocator TYPED(Allocator)
#define Chunk TYPED(Chunk)

static_assert(sizeof(TYPE) >= sizeof(uint32_t),
              "TYPE must be large enough to hold the free list index");

typedef struct Chunk {
    // Aligned to CHUNK_SIZE for fast pointer-to-chunk lookup
    TYPE memory[CHUNK_CAPACITY];
    // Head of the free list (index into `memory`)
    uint32_t free_list;
    // Number of used items in `memory`
    uint32_t used_count;
    // Doubly linked list pointers
    struct Chunk* prev;
    struct Chunk* next;
} Chunk;

typedef struct Allocator {
    // Linked list of chunks with free space
    Chunk* free_chunks;
    // Linked list of full chunks
    Chunk* full_chunks;
} Allocator;

// Allocates a block of memory of `size` bytes with `alignment` alignment.
// Returns NULL on failure.
//
// Memory allocated with this function must be freed with `__aligned_free`.
static void* __aligned_alloc(size_t alignment, size_t size) {
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

// Frees memory allocated with `__aligned_alloc`.
static void __aligned_free(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr); // Why Windows why 😭😭😭
#else
    free(ptr);
#endif
}

// Create a new chunked allocator.
Allocator* TYPED(Allocator_create)(void) {
    Allocator* allocator = malloc(sizeof(Allocator));
    *allocator = (Allocator){
        .free_chunks = NULL,
        .full_chunks = NULL,
    };
    return allocator;
}

// Destroy the chunked allocator and free all memory.
void TYPED(Allocator_destroy)(Allocator* allocator) {
    // Free all free chunks
    for (Chunk* chunk = allocator->free_chunks; chunk != NULL; chunk = chunk->next) {
        free(chunk);
    }
    // Free all full chunks
    for (Chunk* chunk = allocator->full_chunks; chunk != NULL; chunk = chunk->next) {
        free(chunk);
    }
    // Free the allocator itself
    free(allocator);
}

// Prepend `chunk` to the linked list.
void TYPED(__list_prepend)(Chunk** list, Chunk* chunk) {
    chunk->prev = NULL;
    chunk->next = *list;
    if (*list != NULL) {
        (*list)->prev = chunk;
    }
    *list = chunk;
}

// Remove `chunk` from the linked list.
void TYPED(__list_remove)(Chunk** list, Chunk* chunk) {
    if (chunk->prev != NULL) {
        chunk->prev->next = chunk->next;
    }
    if (chunk->next != NULL) {
        chunk->next->prev = chunk->prev;
    }
    if (*list == chunk) {
        *list = chunk->next;
    }
    chunk->prev = NULL;
    chunk->next = NULL;
}

// Allocate a new item from the allocator.
// Returns NULL on failure.
//
// Allocated items must be freed with `Allocator_free`.
TYPE* TYPED(Allocator_alloc)(Allocator* allocator) {
#if defined(__APPLE__)
    return malloc(sizeof(TYPE));
#endif

    // If there are no free chunks, allocate a new chunk
    if (allocator->free_chunks == NULL) {
        Chunk* chunk = __aligned_alloc(CHUNK_SIZE, sizeof(Chunk));
        if (chunk == NULL) {
            return NULL;
        }
        // Initialise free list
        for (uint32_t i = 0; i < CHUNK_CAPACITY - 1; i++) {
            uint32_t* ptr = (uint32_t*)&chunk->memory[i];
            *ptr = i + 1;
        }
        chunk->free_list = 0;
        // Prepend to free chunks list
        chunk->used_count = 0;
        TYPED(__list_prepend)(&allocator->free_chunks, chunk);
    }

    // Allocate from the first free chunk
    Chunk* chunk = allocator->free_chunks;
    assert(chunk->used_count < CHUNK_CAPACITY);
    // This address holds the next free index from the free list
    TYPE* item = &chunk->memory[chunk->free_list];
    uint32_t next_index = *(uint32_t*)item;
    // Update the free list head
    chunk->free_list = next_index;
    chunk->used_count++;

    // Move the chunk to the full list if it's now full
    if (chunk->used_count == CHUNK_CAPACITY) {
        TYPED(__list_remove)(&allocator->free_chunks, chunk);
        TYPED(__list_prepend)(&allocator->full_chunks, chunk);
    }

    return item;
}

// Free an item back to the allocator.
void TYPED(Allocator_free)(Allocator* allocator, TYPE* ptr) {
#if defined(__APPLE__)
    free(ptr);
    return;
#endif

    // Copy behaviour of libc's `free` on NULL pointers
    if (ptr == NULL) {
        return;
    }

    // Each chunk is aligned to CHUNK_SIZE.
    // Since CHUNK_SIZE is a power of two, this gives us the chunk's address.
    Chunk* chunk = (Chunk*)((uintptr_t)ptr & CHUNK_MASK);
    bool was_full = (chunk->used_count == CHUNK_CAPACITY);

    // Add the item back to the free list
    uint32_t index = (uint32_t)(ptr - chunk->memory);
    *(uint32_t*)ptr = chunk->free_list;
    chunk->free_list = index;
    chunk->used_count--;

    if (was_full) {
        // Chunk is no longer full, move it back to the free list
        TYPED(__list_remove)(&allocator->full_chunks, chunk);
        TYPED(__list_prepend)(&allocator->free_chunks, chunk);
    }

    if (chunk->used_count == 0) {
        // Chunk is completely unused, free it to not hog memory
        TYPED(__list_remove)(&allocator->free_chunks, chunk);
        __aligned_free(chunk);
    }
}

#undef TYPE
#undef TYPED

#endif
