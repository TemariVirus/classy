#pragma once

#include <stddef.h>
#include <string.h>

// Duplicate a string and its null terminator.
char* strdup(const char* s) {
    char* copy = malloc(strlen(s) + 1);
    if (copy == NULL) {
        return NULL;
    }
    strcpy(copy, s);
    return copy;
}

// Returns whether the string is empty.
bool str_empty(const char* s) { return s[0] == '\0'; }

// Reads characters from `fptr` into `buf`, until the delimiter or EOF is found,
// or until `limit - 1` characters have been read. The delimiter is not included in `buf`.
// Returns whether the delimiter was found.
bool read_until_delim_or_eof(char* buf, size_t limit, const char* delim, FILE* fptr) {
    if (limit == 0) {
        return false;
    }

    size_t delim_len = strlen(delim);
    size_t match_len = 0;
    size_t len = 0;
    while (len < limit - 1) {
        int c = fgetc(fptr);
        if (c == EOF) {
            break;
        }
        buf[len++] = (char)c;

        if (c == delim[match_len]) {
            match_len++;
            if (match_len == delim_len) {
                // Found delimiter
                len -= delim_len;
                buf[len] = '\0';
                return true;
            }
            continue;
        }

        // Find last partial match
        while (match_len > 0) {
            if (strncmp(&buf[len - match_len], delim, match_len) == 0) {
                break;
            }
            match_len--;
        }
    }

    // Delimiter not found
    buf[len] = '\0';
    return false;
}

typedef struct {
    char* current;
} StringSplit;

// Returns a pointer to the next token in the string split by `delim`,
// or NULL if there are no more tokens. If there are multiple consecutive
// delimiters, empty tokens are returned.
//
// To avoid allocating memory, `split->current` is modified.
char* string_split_next(StringSplit* split, const char delim) {
    if (split->current == NULL) {
        return NULL;
    }

    char* token = split->current;
    char* next_delim = strchr(split->current, delim);
    if (next_delim == NULL) {
        split->current = NULL;
    } else {
        *next_delim = '\0';
        split->current = next_delim + 1;
    }
    return token;
}
