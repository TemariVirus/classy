#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "row.c"
#include "tokenizer.c"

// Comparator function for sorting integers in ascending/descending order.
//
// Returns a positive value if `lhs` is greater than `rhs`, 0 if they are equal,
// or a negative value if `lhs` is smaller than `rhs`.
// If `asc` is false, the return value's sign is flipped.
static int __cmp_int(uint32_t lhs, uint32_t rhs, bool asc) {
    int cmp = (lhs > rhs) - (lhs < rhs);
    return asc ? cmp : -cmp;
}

// Comparator function for sorting floats in ascending/descending order.
//
// Returns a positive value if `lhs` is greater than `rhs`, 0 if they are equal,
// or a negative value if `lhs` is smaller than `rhs`.
// If `asc` is false, the return value's sign is flipped.
static int __cmp_float(float lhs, float rhs, bool asc) {
    int cmp = (lhs > rhs) - (lhs < rhs);
    return asc ? cmp : -cmp;
}

// Comparator function for sorting strings in lexicographical ascending/descending order.
//
// Returns a positive value if `lhs` is greater than `rhs`, 0 if they are equal,
// or a negative value if `lhs` is smaller than `rhs`.
// If `asc` is false, the return value's sign is flipped.
static int __cmp_str(const char* lhs, const char* rhs, bool asc) {
    int cmp = strcmp(lhs, rhs);
    return asc ? cmp : -cmp;
}

// Comparator function for sorting records according to a SortBy struct.
//
// To use it, pass it into quicksort() as the `cmp_fn` argument,
// and pass a pointer to the SortBy struct as the `udata` argument.
// If sorting on the ID column, the `temp_id` members of all records must be
// set to their respective IDs.
int cmp_row(const void* l, const void* r, const void* udata) {
    Row* const* lhs = l;
    Row* const* rhs = r;
    const SortBy* sort_by = udata;
    switch (Column_type(sort_by->column)) {
    case VALUE_INT: {
        uint32_t lhs_ui = Row_get_int(*lhs, sort_by->column);
        uint32_t rhs_ui = Row_get_int(*rhs, sort_by->column);
        return __cmp_int(lhs_ui, rhs_ui, sort_by->ascending);
    }
    case VALUE_FLOAT: {
        float lhs_f = Row_get_float(*lhs, sort_by->column);
        float rhs_f = Row_get_float(*rhs, sort_by->column);
        return __cmp_float(lhs_f, rhs_f, sort_by->ascending);
    }
    case VALUE_STRING: {
        char* lhs_s = Row_get_string(*lhs, sort_by->column);
        char* rhs_s = Row_get_string(*rhs, sort_by->column);
        return __cmp_str(lhs_s, rhs_s, sort_by->ascending);
    }
    }
    return 0;
}

// Swaps the first `n` bytes of memory at addresses `a` and `b`.
// `a` and `b` must not be NULL, and the memory regions must not overlap.
static void __swap(void* restrict a, void* restrict b, size_t n) {
    // Copy in chunks of sizeof(size_t) for efficiency
    for (; n >= sizeof(size_t); n -= sizeof(size_t)) {
        size_t temp = *(size_t*)a;
        *(size_t*)a = *(size_t*)b;
        *(size_t*)b = temp;
        a = (char*)a + sizeof(size_t);
        b = (char*)b + sizeof(size_t);
    }
    // Copy remaining bytes one by one
    for (; n > 0; n--) {
        uint8_t temp = *(uint8_t*)a;
        *(uint8_t*)a = *(uint8_t*)b;
        *(uint8_t*)b = temp;
        a = (char*)a + 1;
        b = (char*)b + 1;
    }
}

// Sorts the items in the array `arr` in-place acording to `cmp_fn`. This sort
// is not stable. `len` is the number of items in the array. `item_size` is the
// size of a single item in bytes.
//
// `cmp_fn` should return:
//  - a negative value, if `lhs` should come before `rhs`.
//  - 0, if the relative positions of `lhs` and `rhs` do not matter.
//  - a positive value, if `lhs` should come after `rhs`.
// `udata` is used to pass extra data to `cmp_fn`.
//
// This function has O(n log(n)) average time complexity, O(n^2) worst-case time
// complexity, and O(log(n)) space complexity, where n is the number of items in
// the array.
void quicksort(void* arr, size_t len, size_t item_size,
               int(cmp_fn)(const void* lhs, const void* rhs, const void* udata),
               const void* udata) {
    // Base case
    if (len < 2) {
        return;
    }

    // Pick last element as pivot
    void* pivot = (char*)arr + (len - 1) * item_size;
    // Hoare's Partition Algorithm
    void* head = arr;
    void* tail = (char*)arr + (len - 2) * item_size;
    while (head < tail) {
        // Find leftmost element greater than or equal to pivot
        while (head < tail && cmp_fn(head, pivot, udata) < 0) {
            head = (char*)head + item_size;
        }
        // Find rightmost element less than pivot
        while (tail > head && cmp_fn(tail, pivot, udata) >= 0) {
            tail = (char*)tail - item_size;
        }
        if (head >= tail) {
            break;
        }

        // Swap head and tail
        __swap(head, tail, item_size);
        head = (char*)head + item_size;
        tail = (char*)tail - item_size;
    }

    // Place pivot in its correct position
    if (cmp_fn(head, pivot, udata) < 0) {
        // head is less than pivot, so pivot should go after head
        head = (char*)head + item_size;
    }
    if (head != pivot) {
        __swap(head, pivot, item_size);
        pivot = head;
    }

    // Sort the partitions
    size_t pivot_index = ((char*)pivot - (char*)arr) / item_size;
    quicksort(arr, pivot_index, item_size, cmp_fn, udata);
    quicksort((char*)pivot + item_size, len - pivot_index - 1, item_size, cmp_fn, udata);
}
