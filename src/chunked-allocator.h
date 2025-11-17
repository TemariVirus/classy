// Chunked allocator with O(1) average alloc and free time.
// Worse case alloc/free time matches that of the underlying malloc/free.
// Can only allocate a single fixed size.

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
    // Aligned to CHUNK_SIZE for fast pointer to chunk lookup
    TYPE memory[CHUNK_CAPACITY];
    // Head of the free list (index into `memory`)
    uint32_t free_list;
    // Number of used items in `memory`
    uint32_t used_count;
    struct Chunk* prev;
    struct Chunk* next;
} Chunk;

typedef struct Allocator {
    Chunk* free_chunks;
    Chunk* full_chunks;
} Allocator;

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
    for (Chunk* chunk = allocator->free_chunks; chunk != NULL; chunk = chunk->next) {
        free(chunk);
    }
    for (Chunk* chunk = allocator->full_chunks; chunk != NULL; chunk = chunk->next) {
        free(chunk);
    }
    free(allocator);
}

// Prepend a chunk to the linked list.
void TYPED(__list_prepend)(Chunk** list, Chunk* chunk) {
    chunk->prev = NULL;
    chunk->next = *list;
    if (*list != NULL) {
        (*list)->prev = chunk;
    }
    *list = chunk;
}

// Remove a chunk from the linked list.
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
TYPE* TYPED(Allocator_alloc)(Allocator* allocator) {
    if (allocator->free_chunks == NULL) {
        // We need to allocate a new chunk
#if defined(_MSC_VER)
        Chunk* chunk = _aligned_malloc(sizeof(Chunk), CHUNK_SIZE);
#else
        Chunk* chunk = aligned_alloc(CHUNK_SIZE, sizeof(Chunk));
#endif
        if (chunk == NULL) {
            return NULL;
        }

        chunk->used_count = 0;
        TYPED(__list_prepend)(&allocator->free_chunks, chunk);

        // Initialise free list
        for (uint32_t i = 0; i < CHUNK_CAPACITY - 1; i++) {
            uint32_t* ptr = (uint32_t*)&chunk->memory[i];
            *ptr = i + 1;
        }
        chunk->free_list = 0;
    }

    // Allocate from the first free chunk
    Chunk* chunk = allocator->free_chunks;
    assert(chunk->used_count < CHUNK_CAPACITY);
    // This address holds the next free index from the free list
    TYPE* item = &chunk->memory[chunk->free_list];
    uint32_t next_index = *(uint32_t*)item;
    chunk->free_list = next_index;
    chunk->used_count++;

    if (chunk->used_count == CHUNK_CAPACITY) {
        TYPED(__list_remove)(&allocator->free_chunks, chunk);
        TYPED(__list_prepend)(&allocator->full_chunks, chunk);
    }

    return item;
}

// Free an item back to the allocator.
void TYPED(Allocator_free)(Allocator* allocator, TYPE* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Each chunk is aligned to CHUNK_SIZE
    Chunk* chunk = (Chunk*)((uintptr_t)ptr & CHUNK_MASK);
    bool was_full = (chunk->used_count == CHUNK_CAPACITY);

    // Add the item back to the free list
    uint32_t index = (uint32_t)(ptr - chunk->memory);
    *(uint32_t*)ptr = chunk->free_list;
    chunk->free_list = index;
    chunk->used_count--;

    if (was_full) {
        TYPED(__list_remove)(&allocator->full_chunks, chunk);
        TYPED(__list_prepend)(&allocator->free_chunks, chunk);
    }

    if (chunk->used_count == 0) {
        // Chunk is completely unused, free it to not hog memory
        TYPED(__list_remove)(&allocator->free_chunks, chunk);
        free(chunk);
    }
}

#undef TYPE
#undef TYPED

#endif
