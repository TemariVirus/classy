#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "row.h"
#include "string_helper.h"

typedef enum {
    CMD_OPEN,
    CMD_SHOW_ALL,
    CMD_SHOW_SUMMARY,
    CMD_INSERT,
    CMD_QUERY,
    CMD_UPDATE,
    CMD_DELETE,
    CMD_SAVE,
} CommandTag;

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

typedef struct {
    Column column;
    bool ascending;
} SortBy;

typedef enum {
    // Sorted in order of highest precedence to lowest precedence
    OP_LPAREN = 1,
    OP_RPAREN,
    OP_EQ,
    OP_GT,
    OP_LT,
    OP_IN,
    OP_NOT,
    OP_AND,
    OP_OR,
} OpTag;

typedef enum {
    TOKEN_EOF,
    TOKEN_UNK,
    TOKEN_CMD,
    TOKEN_COLUMN,
    TOKEN_STRING,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_SORT_BY,
    TOKEN_OP,
} TokenTag;

typedef union {
    char* unk;
    CommandTag cmd;
    Column col;
    char* s;
    uint32_t ui;
    float f;
    SortBy sort_by;
    OpTag op;
} TokenData;

typedef struct {
    TokenTag type;
    TokenData data;
} Token;

typedef struct {
    // Pointer to the current character in the input string.
    // NULL if all characters have been consumed.
    char* current;
    // Whether the tokenizer should consume the filename and end.
    bool take_filename_and_end;
} Tokenizer;

// If `*str` starts with `prefix`, advances `*str` past the prefix and returns true.
// Otherwise, `*str` is unchanged and returns false.
bool __str_skip(char** str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    if (strncmp(*str, prefix, prefix_len) == 0) {
        *str += prefix_len;
        return true;
    }
    return false;
}

// Returns the CommandTag represented by the start of the string.
// Updates `str` to point to the character after the command name.
// If no command name is found, `str` is set to NULL.
CommandTag str_to_commandtag(char** str) {
    if (__str_skip(str, "OPEN")) {
        return CMD_OPEN;
    } else if (__str_skip(str, "SHOW ALL")) {
        return CMD_SHOW_ALL;
    } else if (__str_skip(str, "SHOW SUMMARY")) {
        return CMD_SHOW_SUMMARY;
    } else if (__str_skip(str, "INSERT")) {
        return CMD_INSERT;
    } else if (__str_skip(str, "QUERY")) {
        return CMD_QUERY;
    } else if (__str_skip(str, "UPDATE")) {
        return CMD_UPDATE;
    } else if (__str_skip(str, "DELETE")) {
        return CMD_DELETE;
    } else if (__str_skip(str, "SAVE")) {
        return CMD_SAVE;
    }
    *str = NULL;
    return (CommandTag)0;
}

// Returns the name of the given column.
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

// Returns the Column represented by the start of the string.
// Updates `str` to point to the character after the column name.
// If no column name is found, `str` is set to NULL.
Column str_to_column(char** str) {
    if (__str_skip(str, Column_name(COLUMN_ID))) {
        return COLUMN_ID;
    } else if (__str_skip(str, Column_name(COLUMN_NAME))) {
        return COLUMN_NAME;
    } else if (__str_skip(str, Column_name(COLUMN_PROGRAMME))) {
        return COLUMN_PROGRAMME;
    } else if (__str_skip(str, Column_name(COLUMN_MARK))) {
        return COLUMN_MARK;
    }
    *str = NULL;
    return (Column)0;
}

// Returns the SortBy represented by the start of the string.
// Updates `str` to point to the character after the SORT BY clause.
// If no SORT BY clause is found, `str` is set to NULL.
SortBy str_to_sortby(char** str) {
    const SortBy empty = {.column = (Column)0, .ascending = false};

    // SORT BY
    if (!__str_skip(str, "SORT BY ")) {
        *str = NULL;
        return empty;
    }
    // Column name
    Column column = str_to_column(str);
    if (*str == NULL) {
        return empty;
    }
    // Optional ASC/DESC
    bool ascending = true;
    if (__str_skip(str, " ASC")) {
        ascending = true;
    } else if (__str_skip(str, " DESC")) {
        ascending = false;
    }

    return (SortBy){
        .column = column,
        .ascending = ascending,
    };
}

// Returns the OpTag represented by the start of the string.
// Updates `str` to point to the character after the operator.
// If no operator is found, `str` is set to NULL.
OpTag str_to_optag(char** str) {
    if (__str_skip(str, "(")) {
        return OP_LPAREN;
    } else if (__str_skip(str, ")")) {
        return OP_RPAREN;
    } else if (__str_skip(str, "=")) {
        return OP_EQ;
    } else if (__str_skip(str, ">")) {
        return OP_GT;
    } else if (__str_skip(str, "<")) {
        return OP_LT;
    } else if (__str_skip(str, "IN")) {
        return OP_IN;
    } else if (__str_skip(str, "NOT")) {
        return OP_NOT;
    } else if (__str_skip(str, "AND")) {
        return OP_AND;
    } else if (__str_skip(str, "OR")) {
        return OP_OR;
    }
    *str = NULL;
    return (OpTag)0;
}

// Returns the integer represented by the start of the string.
// Updates `str` to point to the character after the integer.
// If no integer is found, `str` is set to NULL.
uint32_t str_to_int(char** str) {
    char* end;
    // Long long should be big enough to hold any 32-bit integer
    long long val = strtoll(*str, &end, 10);
    if (val < 0 || val > UINT32_MAX || end == *str) {
        *str = NULL;
        return 0;
    }
    *str = end;
    return (uint32_t)val;
}

// Returns the float represented by the start of the string.
// Updates `str` to point to the character after the float.
// If no float is found, `str` is set to NULL.
float str_to_float(char** str) {
    char* end;
    float val = strtof(*str, &end);
    if (end == *str) {
        *str = NULL;
        return 0;
    }
    *str = end;
    return val;
}

// Creates a Tokenizer for the given input string.
Tokenizer Tokenizer_create(char* input) {
    return (Tokenizer){
        .current = input,
        .take_filename_and_end = false,
    };
}

// Reads the next token from the tokenizer.
Token Tokenizer_next(Tokenizer* tokenizer) {
    if (tokenizer->current == NULL) {
        return (Token){.type = TOKEN_EOF};
    }
    // Ignore whitespace between tokens
    while (isspace(tokenizer->current[0])) {
        tokenizer->current++;
    }
    // End of input string
    if (tokenizer->current[0] == '\0') {
        tokenizer->current = NULL;
        return (Token){.type = TOKEN_EOF};
    }
    // Filenames are not escaped or quoted
    if (tokenizer->take_filename_and_end) {
        Token token = (Token){.type = TOKEN_STRING, .data.s = tokenizer->current};
        tokenizer->current = NULL;
        return token;
    }
    // Regular string
    if (tokenizer->current[0] == '"') {
        char* escaped = read_escaped_string(&tokenizer->current);
        if (escaped == NULL) {
            Token token = {.type = TOKEN_UNK, .data.unk = tokenizer->current};
            tokenizer->current = NULL;
            return token;
        }
        return (Token){
            .type = TOKEN_STRING,
            .data.s = escaped,
        };
    }

    // Command
    char* end = tokenizer->current;
    CommandTag cmd = str_to_commandtag(&end);
    if (end != NULL) {
        tokenizer->current = end;
        if (cmd == CMD_OPEN || cmd == CMD_SAVE) {
            tokenizer->take_filename_and_end = true;
        }
        return (Token){.type = TOKEN_CMD, .data.cmd = cmd};
    }
    // Column
    end = tokenizer->current;
    Column col = str_to_column(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.type = TOKEN_COLUMN, .data.col = col};
    }
    // Sort by
    end = tokenizer->current;
    SortBy sort_by = str_to_sortby(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.type = TOKEN_SORT_BY, .data.sort_by = sort_by};
    }
    // Operator
    end = tokenizer->current;
    OpTag op = str_to_optag(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.type = TOKEN_OP, .data.op = op};
    }
    // Integer
    end = tokenizer->current;
    uint32_t int_val = str_to_int(&end);
    // If there was a trailing '.', this is a float, not an int
    if (end != NULL && end[0] != '.') {
        tokenizer->current = end;
        return (Token){.type = TOKEN_INT, .data.ui = int_val};
    }
    // Float
    end = tokenizer->current;
    float float_val = str_to_float(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.type = TOKEN_FLOAT, .data.f = float_val};
    }

    // Nothing matched, return unknown token
    Token token = {.type = TOKEN_UNK, .data.unk = tokenizer->current};
    // The remaining input was consumed as an unknown token,
    // so we set current to NULL to make future calls return EOF.
    tokenizer->current = NULL;
    return token;
}
