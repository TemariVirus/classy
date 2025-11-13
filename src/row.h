#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "string_helper.h"

typedef uint32_t ID;
typedef struct {
    char* name;
    char* programme;
    float mark;
} Row;

// Parse an ID from a string. Returns whether parsing was successful.
bool parse_id(const char* str, ID* out_id) {
    if (str == NULL) {
        return false;
    }
    char* end_ptr;
    // long long should be at least 32 bits on any reasonable platform
    unsigned long long val = strtoull(str, &end_ptr, 10);
    if (val > UINT32_MAX || !str_empty(end_ptr)) {
        return false;
    }
    *out_id = (ID)val;
    return true;
}

// Parse a float from a string. Returns whether parsing was successful.
bool parse_float(const char* str, float* out_mark) {
    // C's string APIs all suck because they use null terminators
    // instead of storing the length, dammit.
    if (str == NULL) {
        return false;
    }
    char* end_ptr;
    float val = strtof(str, &end_ptr);
    if (!str_empty(end_ptr)) {
        return false;
    }
    *out_mark = val;
    return true;
}

// Read a string enclosed in double quotes.
// Returns a pointer to the end of the string, or NULL on failure.
//
// `str` is modified to escape '"' and '\', and to add a null terminator.
char* read_string(char* str) {
    if (str == NULL || str[0] != '"') {
        return NULL;
    }

    size_t read_idx = 1, write_idx = 0;
    while (str[read_idx] != '\0') {
        switch (str[read_idx]) {
        case '"':
            // End of string
            str[write_idx] = '\0';
            return &str[read_idx + 1];
        case '\\':
            // Escape character
            read_idx++;
            switch (str[read_idx]) {
            case '"':
            case '\\':
                str[write_idx++] = str[read_idx++];
                break;
            default:
                // Invalid escape sequence
                return NULL;
            }
            break;
        default:
            // Normal character
            str[write_idx++] = str[read_idx++];
            break;
        }
    }
    // Quote not closed
    return NULL;
}

// Duplicate a row and its pointers.
Row Row_dupe(const Row* row) {
    return (Row){
        // TODO: these allocations take up a significant amount of total runtime.
        // There has to be a better way.
        .name = strdup(row->name),
        .programme = strdup(row->programme),
        .mark = row->mark,
    };
}

// Free the pointers of a row.
void Row_destroy(Row* row) {
    free(row->name);
    free(row->programme);
}
