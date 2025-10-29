#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t ID;
typedef struct {
    char* name;
    char* programme;
    float mark;
} Row;

// Duplicate a string and its null terminator.
char* __strdup(char* s) {
    size_t len = strlen(s);
    char* copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

// Duplicate a row and its pointers.
Row Row_dupe(const Row* row) {
    return (Row){
        // TODO: these allocations take up a significant amount of total runtime.
        // There has to be a better way.
        .name = __strdup(row->name),
        .programme = __strdup(row->programme),
        .mark = row->mark,
    };
}

// Free the pointers of a row.
void Row_destroy(Row* row) {
    free(row->name);
    free(row->programme);
}
