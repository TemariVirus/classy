#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    COLUMN_ID = 0,
    COLUMN_NAME = 1,
    COLUMN_PROGRAMME = 2,
    COLUMN_MARK = 3,
} Column;
#define COLUMN_COUNT 4

// The COLUMN_* values indicate the bit position.
typedef uint8_t ColumnsMask;
#define ALL_COLUMNS_MASK (((ColumnsMask)1 << COLUMN_COUNT) - 1)

const char* Column_name(Column column) {
    switch (column) {
    case COLUMN_ID:
        return "ID";
    case COLUMN_NAME:
        return "Name";
    case COLUMN_PROGRAMME:
        return "Programme";
    case COLUMN_MARK:
        return "Mark";
    }
    assert(false); // Unreachable
}

// Returns true if `name` corresponds to a valid Column, false otherwise.
// If true, the corresponding Column is written to `out`.
bool Column_from_name(const char* name, Column* out) {
    if (strcmp(name, Column_name(COLUMN_ID)) == 0) {
        *out = COLUMN_ID;
    } else if (strcmp(name, Column_name(COLUMN_NAME)) == 0) {
        *out = COLUMN_NAME;
    } else if (strcmp(name, Column_name(COLUMN_PROGRAMME)) == 0) {
        *out = COLUMN_PROGRAMME;
    } else if (strcmp(name, Column_name(COLUMN_MARK)) == 0) {
        *out = COLUMN_MARK;
    } else {
        return false;
    }
    return true;
}
