#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "row.c"
#include "string_helper.c"

typedef enum {
    CMD_HELP,
    CMD_OPEN,
    CMD_SHOW_ALL,
    CMD_SHOW_SUMMARY,
    CMD_INSERT,
    CMD_QUERY,
    CMD_UPDATE,
    CMD_DELETE,
    CMD_SAVE,
} CommandTag;

typedef struct {
    Column column;
    bool ascending;
} SortBy;

typedef enum {
    // Sorted in order of lowest to highest binding power
    OP_OR = 1,
    OP_AND,
    OP_NOT,
    OP_IN,
    OP_LT,
    OP_GT,
    OP_EQ,
    OP_RPAREN,
    OP_LPAREN,
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
    // The active field depends on the tag.
    TokenData data;
    TokenTag tag;
} Token;

typedef struct {
    // The last token that was read. Used for peeking.
    Token last_token;
    // Pointer to the current character in the input string.
    // NULL if all characters have been consumed.
    char* current;
    // Whether the tokenizer should consume the filename and end.
    bool take_filename_and_end;
} Tokenizer;

// The default sort by arguments used when none are specified.
const SortBy DEFAULT_SORT_BY = {
    .column = COLUMN_ID,
    .ascending = true,
};

// If `*str` starts with `prefix`, advances `*str` past the prefix and returns
// true. Otherwise, `*str` is unchanged and returns false.
static bool __str_skip(char** str, const char* prefix) {
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
    CommandTag cmd;
    if (__str_skip(str, "HELP")) {
        cmd = CMD_HELP;
    } else if (__str_skip(str, "OPEN")) {
        cmd = CMD_OPEN;
    } else if (__str_skip(str, "SHOW ALL")) {
        cmd = CMD_SHOW_ALL;
    } else if (__str_skip(str, "SHOW SUMMARY")) {
        cmd = CMD_SHOW_SUMMARY;
    } else if (__str_skip(str, "INSERT")) {
        cmd = CMD_INSERT;
    } else if (__str_skip(str, "QUERY")) {
        cmd = CMD_QUERY;
    } else if (__str_skip(str, "UPDATE")) {
        cmd = CMD_UPDATE;
    } else if (__str_skip(str, "DELETE")) {
        cmd = CMD_DELETE;
    } else if (__str_skip(str, "SAVE")) {
        cmd = CMD_SAVE;
    } else {
        goto fail;
    }

    // Command must be followed by whitespace or end of string
    if (isspace(*str[0]) || *str[0] == '\0') {
        return cmd;
    }

fail:
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
}

// Returns the Column represented by the start of the string.
// Updates `str` to point to the character after the column name.
// If no column name is found, `str` is set to NULL.
Column str_to_column(char** str) {
    Column col;
    if (__str_skip(str, Column_name(COLUMN_ID))) {
        col = COLUMN_ID;
    } else if (__str_skip(str, Column_name(COLUMN_NAME))) {
        col = COLUMN_NAME;
    } else if (__str_skip(str, Column_name(COLUMN_PROGRAMME))) {
        col = COLUMN_PROGRAMME;
    } else if (__str_skip(str, Column_name(COLUMN_MARK))) {
        col = COLUMN_MARK;
    } else {
        goto fail;
    }
    if (!isalpha(*str[0])) {
        return col;
    }

fail:
    *str = NULL;
    return (Column)0;
}

// Returns the SortBy represented by the start of the string.
// Updates `str` to point to the character after the SORT BY clause.
// If no SORT BY clause is found, `str` is set to NULL.
SortBy str_to_sortby(char** str) {
    SortBy sort_by = DEFAULT_SORT_BY;

    // SORT BY
    if (!__str_skip(str, "SORT BY ")) {
        *str = NULL;
        return sort_by;
    }
    // Column name
    sort_by.column = str_to_column(str);
    if (*str == NULL) {
        return sort_by;
    }
    // Optional ASC/DESC
    if (__str_skip(str, " ASC")) {
        sort_by.ascending = true;
    } else if (__str_skip(str, " DESC")) {
        sort_by.ascending = false;
    }

    return sort_by;
}

// Returns the OpTag represented by the start of the string.
// Updates `str` to point to the character after the operator.
// If no operator is found, `str` is set to NULL.
OpTag str_to_optag(char** str) {
    OpTag op;
    bool allow_alpha_after;
    if (__str_skip(str, "(")) {
        allow_alpha_after = true;
        op = OP_LPAREN;
    } else if (__str_skip(str, ")")) {
        allow_alpha_after = true;
        op = OP_RPAREN;
    } else if (__str_skip(str, "=")) {
        allow_alpha_after = true;
        op = OP_EQ;
    } else if (__str_skip(str, ">")) {
        allow_alpha_after = true;
        op = OP_GT;
    } else if (__str_skip(str, "<")) {
        allow_alpha_after = true;
        op = OP_LT;
    } else if (__str_skip(str, "IN")) {
        allow_alpha_after = false;
        op = OP_IN;
    } else if (__str_skip(str, "NOT")) {
        allow_alpha_after = false;
        op = OP_NOT;
    } else if (__str_skip(str, "AND")) {
        allow_alpha_after = false;
        op = OP_AND;
    } else if (__str_skip(str, "OR")) {
        allow_alpha_after = false;
        op = OP_OR;
    } else {
        goto fail;
    }
    if (allow_alpha_after || !isalpha(*str[0])) {
        return op;
    }

fail:
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
    // If one of these characters follows, it's a float
    if (end[0] != '\0' && strchr(".eE", end[0]) != NULL) {
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

// This function is not meant to be called directly.
// See `Tokenizer_next`.
static Token __Tokenizer_next_inner(Tokenizer* tokenizer) {
    if (tokenizer->current == NULL) {
        return (Token){.tag = TOKEN_EOF};
    }

    // Filenames are not escaped or quoted
    if (tokenizer->take_filename_and_end) {
        // Filename cannot be empty
        if (tokenizer->current[0] == '\0') {
            tokenizer->current = NULL;
            return (Token){.tag = TOKEN_EOF};
        }

        Token token = (Token){.tag = TOKEN_STRING, .data.s = tokenizer->current};
        tokenizer->current = NULL;
        return token;
    }
    // Ignore whitespace between tokens
    while (isspace(tokenizer->current[0])) {
        tokenizer->current++;
    }
    // End of input string
    if (tokenizer->current[0] == '\0') {
        tokenizer->current = NULL;
        return (Token){.tag = TOKEN_EOF};
    }
    // Regular string
    if (tokenizer->current[0] == '"') {
        char* escaped = read_escaped_string(&tokenizer->current);
        if (escaped == NULL) {
            Token token = {.tag = TOKEN_UNK, .data.unk = tokenizer->current};
            tokenizer->current = NULL;
            return token;
        }
        return (Token){
            .tag = TOKEN_STRING,
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
            if (isspace(tokenizer->current[0])) {
                // Skip whitespace after command
                tokenizer->current++;
            }
        }
        return (Token){.tag = TOKEN_CMD, .data.cmd = cmd};
    }
    // Column
    end = tokenizer->current;
    Column col = str_to_column(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.tag = TOKEN_COLUMN, .data.col = col};
    }
    // Sort by
    end = tokenizer->current;
    SortBy sort_by = str_to_sortby(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.tag = TOKEN_SORT_BY, .data.sort_by = sort_by};
    }
    // Operator
    end = tokenizer->current;
    OpTag op = str_to_optag(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.tag = TOKEN_OP, .data.op = op};
    }
    // Integer
    end = tokenizer->current;
    uint32_t int_val = str_to_int(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.tag = TOKEN_INT, .data.ui = int_val};
    }
    // Float
    end = tokenizer->current;
    float float_val = str_to_float(&end);
    if (end != NULL) {
        tokenizer->current = end;
        return (Token){.tag = TOKEN_FLOAT, .data.f = float_val};
    }

    // Nothing matched, return unknown token
    Token token = {.tag = TOKEN_UNK, .data.unk = tokenizer->current};
    // The remaining input was consumed as an unknown token,
    // so we set current to NULL to make future calls return EOF.
    tokenizer->current = NULL;
    return token;
}

// Creates a Tokenizer for the given input string.
Tokenizer Tokenizer_create(char* input) {
    Tokenizer tokenizer = (Tokenizer){
        .current = input,
        .take_filename_and_end = false,
    };
    tokenizer.last_token = __Tokenizer_next_inner(&tokenizer);
    return tokenizer;
}

// Reads the next token without advancing the tokenizer.
Token Tokenizer_peek(Tokenizer* tokenizer) { return tokenizer->last_token; }

// Reads the next token and advances the tokenizer.
Token Tokenizer_next(Tokenizer* tokenizer) {
    Token token = tokenizer->last_token;
    tokenizer->last_token = __Tokenizer_next_inner(tokenizer);
    return token;
}
