// A type-safe growable list with dynamic memory allocation.

#ifdef TYPE

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "panic.c"

#ifndef TYPED
#define TYPED(THING) THING
#endif

typedef struct {
    TYPE* items;
    size_t length;
    size_t capacity;
} TYPED(List);

// Creates an empty list. The list should be freed with `List_destroy`.
//
// Also see `List_from_buffer`.
TYPED(List) TYPED(List_create)(void) {
    return (TYPED(List)){
        .items = NULL,
        .length = 0,
        .capacity = 0,
    };
}

// Creates an empty list backed by the given buffer.
// `buffer` must be large enough to hold `capacity` elements.
//
// Also see `List_create`.
TYPED(List) TYPED(List_from_buffer)(TYPE* buffer, size_t capacity) {
    return (TYPED(List)){
        .items = buffer,
        .length = 0,
        .capacity = capacity,
    };
}

// Frees all memory used by the list.
// The list may be reused after calling this function.
void TYPED(List_destroy)(TYPED(List) * self) {
    if (self->items != NULL) {
        free(self->items);
        self->items = NULL;
    }
    self->length = 0;
    self->capacity = 0;
}

// Clears the list without freeing memory.
void TYPED(List_clear)(TYPED(List) * self) { self->length = 0; }

// Ensure that the list has at least the specified capacity.
void TYPED(List_ensure_capacity)(TYPED(List) * self, size_t new_capacity) {
    size_t capacity = self->capacity;
    while (capacity < new_capacity) {
        capacity = capacity * 2 + 1;
    }
    if (capacity > self->capacity) {
        self->items = realloc(self->items, capacity * sizeof(TYPE));
        if (self->items == NULL) {
            PANIC("Out of memory.");
        }
        self->capacity = capacity;
    }
}

// Get the element at the specified index.
TYPE TYPED(List_get)(const TYPED(List) * self, size_t index) {
    assert(index < self->length);
    return self->items[index];
}

// Set the element at the specifed index.
void TYPED(List_set)(TYPED(List) * self, size_t index, TYPE value) {
    assert(index < self->length);
    self->items[index] = value;
}

// Append an element to the end of the list without checking capacity.
// This function never allocates memory.
//
// Also see `List_append`.
void TYPED(List_append_assume_capacity)(TYPED(List) * self, TYPE item) {
    assert(self->length < self->capacity);
    self->items[self->length++] = item;
}

// Append an element to the end of the list.
//
// Also see `List_append_assume_capacity`.
void TYPED(List_append)(TYPED(List) * self, TYPE item) {
    TYPED(List_ensure_capacity)(self, self->length + 1);
    TYPED(List_append_assume_capacity)(self, item);
}

// Pop an element from the end of the list.
//
// Also see `List_remove`.
TYPE TYPED(List_pop)(TYPED(List) * self) {
    assert(self->length > 0);
    return self->items[--self->length];
}

// Remove an element from the specified index,
// maintaining the order of the remaining elements.
TYPE TYPED(List_remove)(TYPED(List) * self, size_t index) {
    assert(index < self->length);

    TYPE item = self->items[index];
    memmove(&self->items[index], &self->items[index + 1],
            (self->length - index - 1) * sizeof(TYPE));
    self->length--;
    return item;
}

#undef TYPE
#undef TYPED

#endif
