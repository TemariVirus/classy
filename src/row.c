#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "string_helper.c"
#include "unreachable.c"

typedef enum {
    VALUE_INT,
    VALUE_FLOAT,
    VALUE_STRING,
} ValueTag;

typedef enum {
    COLUMN_ID = 0,
    COLUMN_NAME = 1,
    COLUMN_PROGRAMME = 2,
    COLUMN_MARK = 3,
} Column;
#define COLUMN_COUNT 4

// The COLUMN_* values indicate the bit position.
typedef uint8_t ColumnsMask;
#define COLUMNS_MASK_EMPTY 0
#define COLUMNS_MASK_FULL (((ColumnsMask)1 << COLUMN_COUNT) - 1)

typedef uint32_t ID;
typedef struct {
    char* name;
    char* programme;
    float mark;
    // This member is unused padding and carries no meaning.
    // However, it may be used by some operations to avoid memory allocations.
    uint32_t temp_id;
} Row;

// The data type of a column.
ValueTag Column_type(Column column) {
    switch (column) {
    case COLUMN_ID:
        return VALUE_INT;
    case COLUMN_MARK:
        return VALUE_FLOAT;
    case COLUMN_NAME:
    case COLUMN_PROGRAMME:
        return VALUE_STRING;
    }
}

// Returns whether the bit of the specified column is set.
bool ColumnsMask_get(const ColumnsMask* mask, Column column) {
    return (*mask & (1 << column)) != 0;
}

// Sets the bit of the specified column.
void ColumnsMask_set(ColumnsMask* mask, Column column) { *mask |= (1 << column); }

// Unsets the bit of the specified column.
void ColumnsMask_unset(ColumnsMask* mask, Column column) { *mask &= ~(1 << column); }

// Parse an ID from a string. Returns whether parsing was successful.
bool parse_id(const char* str, ID* out_id) {
    if (str == NULL || str_empty(str)) {
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
    if (str == NULL || str_empty(str)) {
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

// Read and escapes a string enclosed in double quotes.
// Returns a pointer to the start of the string, or NULL on failure.
//
// On success, `str_ptr` is updated to point to the character after the closing quote.
// `*str_ptr` is modified to escape '"' and '\', and to add a null terminator.
char* read_escaped_string(char** str_ptr) {
    char* str = *str_ptr;
    if (str == NULL || str[0] != '"') {
        return NULL;
    }

    size_t read_idx = 1, write_idx = 0;
    while (str[read_idx] != '\0') {
        switch (str[read_idx]) {
        case '"':
            // End of string
            str[write_idx] = '\0';
            *str_ptr = &str[read_idx + 1];
            return str;
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

// Returns the value of the specified column of the given row.
// The data type of `column` must be VALUE_INT.
// The `temp_id` member must be set to the row's ID.
uint32_t Row_get_int(const Row* row, Column column) {
    (void)row;
    switch (column) {
    case COLUMN_ID:
        return row->temp_id;
    case COLUMN_NAME:
    case COLUMN_PROGRAMME:
    case COLUMN_MARK:
        UNREACHABLE;
    }
}

// Returns the value of the specified column of the given row.
// The data type of `column` must be VALUE_FLOAT.
float Row_get_float(const Row* row, Column column) {
    switch (column) {
    case COLUMN_MARK:
        return row->mark;
    case COLUMN_ID:
    case COLUMN_NAME:
    case COLUMN_PROGRAMME:
        UNREACHABLE;
    }
}

// Returns the value of the specified column of the given row.
// The data type of `column` must be VALUE_STRING.
char* Row_get_string(const Row* row, Column column) {
    switch (column) {
    case COLUMN_NAME:
        return row->name;
    case COLUMN_PROGRAMME:
        return row->programme;
    case COLUMN_ID:
    case COLUMN_MARK:
        UNREACHABLE;
    }
}

// Duplicate a row and its pointers.
Row Row_dupe(const Row* row) {
    return (Row){
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
