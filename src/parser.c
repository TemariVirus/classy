#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "condition.c"
#include "error.c"
#include "row.c"
#include "tokenizer.c"
#include "unreachable.c"

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STRING,
    EXPR_COLUMN,
    EXPR_CONDITION,
} ExpressionTag;

// An expression is either an atomic value within a condition, or a condition itself.
typedef struct {
    // The active field depends on the tag.
    union {
        uint32_t ui;
        float f;
        char* s;
        Column column;
        Condition* cond;
    } value;
    ExpressionTag tag;
} Expression;

typedef struct {
    // Name of the file to open. Cannot be NULL.
    // This struct does not own this string.
    char* filename;
} CmdOpenArgs;

typedef struct {
    // Name of the file to save to. Can be NULL.
    // This struct does not own this string.
    char* filename;
} CmdSaveArgs;

typedef struct {
    // How to sort the rows.
    SortBy sort_by;
} CmdShowAllArgs;

typedef struct {
    // Condition to filter rows. If NULL, no filtering is applied.
    Condition* filter;
} CmdSummaryArgs;

typedef struct {
    // Condition to filter rows. Cannot be NULL.
    Condition* filter;
    SortBy sort_by;
} CmdQueryArgs;

typedef struct {
    // The row to insert.
    Row row;
    // The ID of the new row.
    ID id;
} CmdInsertArgs;

typedef struct {
    // The row with the updated values.
    Row row;
    // The ID of the row to update.
    ID id;
    // Columns that should be updated have their bit set to 1.
    ColumnsMask update_columns;
} CmdUpdateArgs;

typedef struct {
    // The ID of the row to delete.
    ID id;
} CmdDeleteArgs;

// A command and the arguments needed to execute it.
typedef struct {
    union {
        CmdOpenArgs open;
        CmdSaveArgs save;
        CmdShowAllArgs show_all;
        CmdSummaryArgs show_summary;
        CmdQueryArgs query;
        CmdInsertArgs insert;
        CmdUpdateArgs update;
        CmdDeleteArgs delete;
    } args;
    CommandTag tag;
} Command;

typedef enum {
    parse_command__ok = ERROR_OK,
    parse_command__expected_eof = ERROR_EXPECTED_EOF,
    parse_command__expected_command = ERROR_EXPECTED_COMMAND,
    parse_command__expected_value = ERROR_EXPECTED_VALUE,
    parse_command__expected_int = ERROR_EXPECTED_INT,
    parse_command__expected_float = ERROR_EXPECTED_FLOAT,
    parse_command__expected_str = ERROR_EXPECTED_STR,
    parse_command__expected_col = ERROR_EXPECTED_COL,
    parse_command__expected_id_col = ERROR_EXPECTED_ID_COL,
    parse_command__expected_str_col = ERROR_EXPECTED_STR_COL,
    parse_command__expected_op_eq = ERROR_EXPECTED_OP_EQ,
    parse_command__expected_cond = ERROR_EXPECTED_COND,
    parse_command__duplicate_col = ERROR_DUPLICATE_COL,
    parse_command__mismatched_paren = ERROR_MISMATCHED_PAREN,
    parse_command__bad_op = ERROR_BAD_OP,
    parse_command__out_of_mem = ERROR_OUT_OF_MEM,
} parse_command__Error;

static parse_command__Error __parse_expression(Tokenizer*, uint8_t, Expression*);

// Writes the right binding power of a prefix operator to `rbp`.
// Returns whether `op` is a valid prefix operator.
static bool __prefix_binding_power(OpTag op, uint8_t* rbp) {
    switch (op) {
    // Not prefix
    case OP_LPAREN:
    case OP_RPAREN:
    case OP_EQ:
    case OP_GT:
    case OP_LT:
    case OP_IN:
    case OP_AND:
    case OP_OR:
        return false;
    // Prefix
    case OP_NOT:
        *rbp = (uint8_t)op * 2;
        return true;
    }
}

// Writes the left and right binding powers of an infix operator to `lbp` and `rbp`.
// Returns whether `op` is a valid infix operator.
static bool __infix_binding_power(OpTag op, uint8_t* lbp, uint8_t* rbp) {
    switch (op) {
    // Not infix
    case OP_LPAREN:
    case OP_RPAREN:
    case OP_NOT:
        return false;
    // Infix, left-associative
    case OP_EQ:
    case OP_GT:
    case OP_LT:
    case OP_IN:
    case OP_AND:
    case OP_OR:
        *lbp = (uint8_t)op * 2 - 1;
        *rbp = (uint8_t)op * 2;
        return true;
    }
}

// Returns whether the operator `op` supports the left operand `lhs`.
static bool __op_supports_lhs(OpTag op, Expression lhs) {
    switch (op) {
    case OP_EQ:
        return lhs.tag == EXPR_COLUMN;
    case OP_GT:
    case OP_LT:
        if (lhs.tag != EXPR_COLUMN) {
            return false;
        }
        switch (Column_type(lhs.value.column)) {
        case VALUE_INT:
        case VALUE_FLOAT:
            return true;
        default:
            return false;
        }
    case OP_IN:
        return lhs.tag == EXPR_STRING;
    case OP_AND:
    case OP_OR:
        return lhs.tag == EXPR_CONDITION;
    default:
        return false;
    }
}

// Attempts to coerce `rhs` into a value suitable for comparison with `lhs_column`.
// Writes the coerced value to `out_value`.
static parse_command__Error __coerce_eq_gt_lt_value(Column lhs_column, Expression rhs,
                                                    union EqGtLtValue* out_value) {
    switch (Column_type(lhs_column)) {
    case VALUE_INT:
        switch (rhs.tag) {
        case EXPR_INT:
            out_value->ui = rhs.value.ui;
            return parse_command__ok;
        default:
            return parse_command__expected_int;
        }
    case VALUE_FLOAT:
        switch (rhs.tag) {
        case EXPR_INT:
            out_value->f = rhs.value.ui;
            return parse_command__ok;
        case EXPR_FLOAT:
            out_value->f = rhs.value.f;
            return parse_command__ok;
        default:
            return parse_command__expected_float;
        }
    case VALUE_STRING:
        switch (rhs.tag) {
        case EXPR_STRING:
            out_value->s = rhs.value.s;
            return parse_command__ok;
        default:
            return parse_command__expected_str;
        }
    }
}

// Free all memory used by an expression.
static void __expression_destroy(Expression* expr) {
    if (expr == NULL) {
        return;
    }
    switch (expr->tag) {
    case EXPR_CONDITION:
        Condition_destroy(expr->value.cond);
        break;
    default:
        break;
    }
}

// Free all memory used by a command.
void Command_destroy(Command* cmd) {
    if (cmd == NULL) {
        return;
    }
    switch (cmd->tag) {
    case CMD_SHOW_SUMMARY:
        Condition_destroy(cmd->args.show_summary.filter);
        break;
    case CMD_QUERY:
        Condition_destroy(cmd->args.query.filter);
        break;
    default:
        break;
    }
}

// Convert a token to an atomic expression.
// `token.tag` must be TOKEN_COLUMN, TOKEN_STRING, TOKEN_INT or TOKEN_FLOAT.
static Expression __expression_from_token(Token token) {
    switch (token.tag) {
    case TOKEN_COLUMN:
        return (Expression){
            .tag = EXPR_COLUMN,
            .value.column = token.data.col,
        };
    case TOKEN_STRING:
        return (Expression){
            .tag = EXPR_STRING,
            .value.s = token.data.s,
        };
    case TOKEN_INT:
        return (Expression){
            .tag = EXPR_INT,
            .value.ui = token.data.ui,
        };
    case TOKEN_FLOAT:
        return (Expression){
            .tag = EXPR_FLOAT,
            .value.f = token.data.f,
        };
    default:
        UNREACHABLE;
    }
}

// Create a condition at `out_expr` with the prefix operator `op`,
// right binding power `rbp`, and operand parsed from `tokenizer`.
static parse_command__Error __prefixed_condition(OpTag op, uint8_t rbp, Tokenizer* tokenizer,
                                                 Expression* out_expr) {
    parse_command__Error err;
    Condition* cond = NULL;
    Expression rhs;
    bool also_free_rhs = false;

    // Get operand
    err = __parse_expression(tokenizer, rbp, &rhs);
    if (err != parse_command__ok) {
        goto error_cleanup;
    }
    also_free_rhs = true;

    // Construct condition with prefix operator
    cond = malloc(sizeof(Condition));
    if (cond == NULL) {
        err = parse_command__out_of_mem;
        goto error_cleanup;
    }
    cond->tag = op;
    switch (op) {
    case OP_NOT:
        if (rhs.tag != EXPR_CONDITION) {
            err = parse_command__expected_cond;
            goto error_cleanup;
        }
        cond->args.not.cond = rhs.value.cond;
        also_free_rhs = false;
        break;
    default:
        UNREACHABLE;
    }

    // Wrap condition in expression
    *out_expr = (Expression){
        .tag = EXPR_CONDITION,
        .value.cond = cond,
    };
    return parse_command__ok;

error_cleanup:
    free(cond);
    if (also_free_rhs) {
        __expression_destroy(&rhs);
    }
    return err;
}

// Create a condition at `out_expr` with the left operand `lhs`, infix operator `op`,
// right binding power `rbp`, and right operand parsed from `tokenizer`.
static parse_command__Error __infixed_condition(Expression lhs, OpTag op, uint8_t rbp,
                                                Tokenizer* tokenizer, Expression* out_expr) {
    parse_command__Error err;
    Condition* cond = NULL;
    Expression rhs;
    bool also_free_rhs = false;

    // Get second operand (lhs is the first operand)
    err = __parse_expression(tokenizer, rbp, &rhs);
    if (err != parse_command__ok) {
        goto error_cleanup;
    }
    also_free_rhs = true;

    // Construct condition with infix operator
    cond = malloc(sizeof(Condition));
    if (cond == NULL) {
        err = parse_command__out_of_mem;
        goto error_cleanup;
    }
    cond->tag = op;
    switch (op) {
    case OP_EQ:
    case OP_GT:
    case OP_LT:
        if (lhs.tag != EXPR_COLUMN) {
            err = parse_command__expected_col;
            goto error_cleanup;
        }
        cond->args.eq_gt_lt.column = lhs.value.column;
        err = __coerce_eq_gt_lt_value(lhs.value.column, rhs, &cond->args.eq_gt_lt.value);
        if (err != parse_command__ok) {
            goto error_cleanup;
        }
        break;
    case OP_IN:
        if (rhs.tag != EXPR_COLUMN || Column_type(rhs.value.column) != VALUE_STRING) {
            err = parse_command__expected_str_col;
            goto error_cleanup;
        }
        cond->args.in.column = rhs.value.column;
        if (lhs.tag != EXPR_STRING) {
            err = parse_command__expected_str;
            goto error_cleanup;
        }
        cond->args.in.value = lhs.value.s;
        break;
    case OP_AND:
    case OP_OR:
        if (rhs.tag != EXPR_CONDITION) {
            err = parse_command__expected_cond;
            goto error_cleanup;
        }
        cond->args.and_or.rhs = rhs.value.cond;
        also_free_rhs = false;
        if (lhs.tag != EXPR_CONDITION) {
            err = parse_command__expected_cond;
            goto error_cleanup;
        }
        cond->args.and_or.lhs = lhs.value.cond;
        break;
    default:
        UNREACHABLE;
    }

    // Wrap condition in expression
    *out_expr = (Expression){
        .tag = EXPR_CONDITION,
        .value.cond = cond,
    };
    return parse_command__ok;

error_cleanup:
    free(cond);
    if (also_free_rhs) {
        __expression_destroy(&rhs);
    }
    return err;
}

// Pratt parser for expressions.
// Parses an expression into `out_expr` from `tokenizer` until the next operator
// with left binding power less than `min_bp` is encountered.
//
// See: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
static parse_command__Error __parse_expression(Tokenizer* tokenizer, uint8_t min_bp,
                                               Expression* out_expr) {
    parse_command__Error err;
    Expression lhs;

    // Initialise lhs
    Token token = Tokenizer_next(tokenizer);
    switch (token.tag) {
    case TOKEN_EOF:
    case TOKEN_UNK:
    case TOKEN_CMD:
    case TOKEN_SORT_BY:
        // These tokens cannot be part of an expression
        return parse_command__expected_value;
    case TOKEN_COLUMN:
    case TOKEN_STRING:
    case TOKEN_INT:
    case TOKEN_FLOAT:
        lhs = __expression_from_token(token);
        break;
    case TOKEN_OP:
        switch (token.data.op) {
        case OP_LPAREN:
            // Expression wrapped in parenthesis
            err = __parse_expression(tokenizer, 0, &lhs);
            if (err != parse_command__ok) {
                goto error_cleanup;
            }
            // Must be closed by right parenthesis
            Token next = Tokenizer_next(tokenizer);
            if (next.tag != TOKEN_OP || next.data.op != OP_RPAREN) {
                err = parse_command__mismatched_paren;
                goto error_cleanup;
            }
            break;
        default: {
            // Prefix operator
            uint8_t rbp;
            if (!__prefix_binding_power(token.data.op, &rbp)) {
                // Operation cannot be used as prefix
                return parse_command__bad_op;
            }
            err = __prefixed_condition(token.data.op, rbp, tokenizer, &lhs);
            if (err != parse_command__ok) {
                goto error_cleanup;
            }
            break;
        }
        }
        break;
    }

    // Collect infix operators and rhs expressions into lhs
    while (true) {
        Token op_token = Tokenizer_peek(tokenizer);
        if (op_token.tag != TOKEN_OP) {
            // No more operators, we reached the end of this expression
            break;
        }

        // Infix operator
        uint8_t lbp, rbp;
        if (!__op_supports_lhs(op_token.data.op, lhs) ||
            !__infix_binding_power(op_token.data.op, &lbp, &rbp)) {
            // Operation cannot be used as infix here, we can't parse further
            break;
        }
        if (lbp < min_bp) {
            // Operator binding power is too low, we reached the end of this expression
            break;
        }
        (void)Tokenizer_next(tokenizer); // Discard operator token
        err = __infixed_condition(lhs, op_token.data.op, rbp, tokenizer, &lhs);
        if (err != parse_command__ok) {
            goto error_cleanup;
        }
    }

    *out_expr = lhs;
    return parse_command__ok;

error_cleanup:
    __expression_destroy(&lhs);
    return err;
}

// Consumes tokens from `tokenizer` to parse a condition.
// On success, writes the parsed condition to `out_cond`.
//
// The condition must be freed with `Condition_destroy`.
static parse_command__Error __parse_condition(Tokenizer* tokenizer, Condition** out_cond) {
    Expression expr;
    parse_command__Error err = __parse_expression(tokenizer, 0, &expr);
    if (err != parse_command__ok) {
        return err;
    }
    switch (expr.tag) {
    case EXPR_CONDITION:
        *out_cond = expr.value.cond;
        return parse_command__ok;
    default:
        return parse_command__expected_cond;
    }
}

// Parses at most `n` column-value pairs of the form `Column=value`
// from `tokenizer` into `out_id` and `out_row`.
// A bitmask of the seen columns are written to `out_seen`.
// It is an error for the same column to appear multiple times.
static parse_command__Error __parse_column_values(Tokenizer* tokenizer, int n, ID* out_id,
                                                  Row* out_row, ColumnsMask* out_seen) {
    *out_seen = COLUMNS_MASK_EMPTY;
    for (int i = 0; i < n; i++) {
        Token column_token = Tokenizer_next(tokenizer);
        if (column_token.tag != TOKEN_COLUMN) {
            break;
        }
        Column column = column_token.data.col;
        if (ColumnsMask_get(out_seen, column)) {
            // Column specified multiple times
            return parse_command__duplicate_col;
        }
        ColumnsMask_set(out_seen, column);

        // '=' must separate column and value
        Token eq_token = Tokenizer_next(tokenizer);
        if (eq_token.tag != TOKEN_OP || eq_token.data.op != OP_EQ) {
            return parse_command__expected_op_eq;
        }

        Token value_token = Tokenizer_next(tokenizer);
        switch (value_token.tag) {
        case TOKEN_STRING:
        case TOKEN_INT:
        case TOKEN_FLOAT:
            break;
        default:
            return parse_command__expected_value;
        }

        union EqGtLtValue value;
        parse_command__Error err =
            __coerce_eq_gt_lt_value(column, __expression_from_token(value_token), &value);
        if (err != parse_command__ok) {
            return err;
        }
        switch (column) {
        case COLUMN_ID:
            *out_id = value.ui;
            break;
        case COLUMN_NAME:
            out_row->name = value.s;
            break;
        case COLUMN_PROGRAMME:
            out_row->programme = value.s;
            break;
        case COLUMN_MARK:
            out_row->mark = value.f;
            break;
        }
    }
    return parse_command__ok;
}

// Parses the arguments for the OPEN command from `tokenizer` into `out`.
static parse_command__Error __parse_open(Tokenizer* tokenizer, CmdOpenArgs* out) {
    Token filename_token = Tokenizer_next(tokenizer);
    if (filename_token.tag != TOKEN_STRING) {
        return parse_command__expected_str;
    }
    assert(filename_token.data.s != NULL);
    out->filename = filename_token.data.s;
    return parse_command__ok;
}

// Parses the arguments for the SHOW ALL command from `tokenizer` into `out`.
static parse_command__Error __parse_show_all(Tokenizer* tokenizer, CmdShowAllArgs* out) {
    Token sort_by_token = Tokenizer_next(tokenizer);
    switch (sort_by_token.tag) {
    case TOKEN_EOF:
        // No sort by specified, use default
        out->sort_by = DEFAULT_SORT_BY;
        return parse_command__ok;
    case TOKEN_SORT_BY:
        out->sort_by = sort_by_token.data.sort_by;
        return parse_command__ok;
    default:
        break;
    }
    return parse_command__expected_eof;
}

// Parses the arguments for the SHOW SUMMARY command from `tokenizer` into `out`.
static parse_command__Error __parse_show_summary(Tokenizer* tokenizer, CmdSummaryArgs* out) {
    if (Tokenizer_peek(tokenizer).tag == TOKEN_EOF) {
        // No condition specified
        out->filter = NULL;
        return parse_command__ok;
    }
    return __parse_condition(tokenizer, &out->filter);
}

// Parses the arguments for the INSERT command from `tokenizer` into `out`.
static parse_command__Error __parse_insert(Tokenizer* tokenizer, CmdInsertArgs* out) {
    ColumnsMask seen_columns;
    parse_command__Error err =
        __parse_column_values(tokenizer, COLUMN_COUNT, &out->id, &out->row, &seen_columns);
    if (err != parse_command__ok) {
        return err;
    }
    // All columns are required
    if (seen_columns != COLUMNS_MASK_FULL) {
        return parse_command__expected_col;
    }
    return parse_command__ok;
}

// Parses the arguments for the QUERY command from `tokenizer` into `out`.
static parse_command__Error __parse_query(Tokenizer* tokenizer, CmdQueryArgs* out) {
    parse_command__Error err = __parse_condition(tokenizer, &out->filter);
    if (err != parse_command__ok) {
        return err;
    }

    Token sort_by_token = Tokenizer_next(tokenizer);
    switch (sort_by_token.tag) {
    case TOKEN_EOF:
        // No sort by specified, use default
        out->sort_by = DEFAULT_SORT_BY;
        return parse_command__ok;
    case TOKEN_SORT_BY:
        out->sort_by = sort_by_token.data.sort_by;
        return parse_command__ok;
    default:
        break;
    }
    return parse_command__expected_eof;
}

// Parses the arguments for the UPDATE command from `tokenizer` into `out`.
static parse_command__Error __parse_update(Tokenizer* tokenizer, CmdUpdateArgs* out) {
    ColumnsMask seen_columns;
    parse_command__Error err =
        __parse_column_values(tokenizer, COLUMN_COUNT, &out->id, &out->row, &seen_columns);
    if (err != parse_command__ok) {
        return err;
    }

    // ID column is required
    if (!ColumnsMask_get(&seen_columns, COLUMN_ID)) {
        return parse_command__expected_id_col;
    }
    // Set all seen columns except ID
    ColumnsMask_unset(&seen_columns, COLUMN_ID);
    out->update_columns = seen_columns;
    // At least one column must be updated
    if (out->update_columns == COLUMNS_MASK_EMPTY) {
        return parse_command__expected_col;
    }
    return parse_command__ok;
}

// Parses the arguments for the DELETE command from `tokenizer` into `out`.
static parse_command__Error __parse_delete(Tokenizer* tokenizer, CmdDeleteArgs* out) {
    Token id_col_token = Tokenizer_next(tokenizer);
    if (id_col_token.tag != TOKEN_COLUMN || id_col_token.data.col != COLUMN_ID) {
        return parse_command__expected_id_col;
    }
    Token eq_token = Tokenizer_next(tokenizer);
    if (eq_token.tag != TOKEN_OP || eq_token.data.op != OP_EQ) {
        return parse_command__expected_op_eq;
    }
    Token id_val_token = Tokenizer_next(tokenizer);
    if (id_val_token.tag != TOKEN_INT) {
        return parse_command__expected_int;
    }
    out->id = id_val_token.data.ui;
    return parse_command__ok;
}

// Parses the arguments for the SAVE command from `tokenizer` into `out`.
static parse_command__Error __parse_save(Tokenizer* tokenizer, CmdSaveArgs* out) {
    Token filename_token = Tokenizer_next(tokenizer);
    switch (filename_token.tag) {
    case TOKEN_EOF:
        // No filename specified
        out->filename = NULL;
        return parse_command__ok;
    case TOKEN_STRING:
        assert(filename_token.data.s != NULL);
        out->filename = filename_token.data.s;
        return parse_command__ok;
    default:
        break;
    }
    return parse_command__expected_str;
}

// Consumes all tokens from `tokenizer` to parse a command.
// If parsing is successful, the parsed command is written to `out`.
// Parsing is unsuccessful if there are any tokens remaining after the command is parsed.
//
// The returned command must be freed with `Command_destroy`.
parse_command__Error parse_command(Tokenizer* tokenizer, Command* out) {
    Token cmd_token = Tokenizer_next(tokenizer);
    if (cmd_token.tag != TOKEN_CMD) {
        return parse_command__expected_command;
    }

    parse_command__Error err;
    out->tag = cmd_token.data.cmd;
    switch (cmd_token.data.cmd) {
    case CMD_HELP:
        err = parse_command__ok;
        break;
    case CMD_OPEN:
        err = __parse_open(tokenizer, &out->args.open);
        break;
    case CMD_SHOW_ALL:
        err = __parse_show_all(tokenizer, &out->args.show_all);
        break;
    case CMD_SHOW_SUMMARY:
        out->args.show_summary.filter = NULL;
        err = __parse_show_summary(tokenizer, &out->args.show_summary);
        break;
    case CMD_INSERT:
        err = __parse_insert(tokenizer, &out->args.insert);
        break;
    case CMD_QUERY:
        out->args.query.filter = NULL;
        err = __parse_query(tokenizer, &out->args.query);
        break;
    case CMD_UPDATE:
        err = __parse_update(tokenizer, &out->args.update);
        break;
    case CMD_DELETE:
        err = __parse_delete(tokenizer, &out->args.delete);
        break;
    case CMD_SAVE:
        err = __parse_save(tokenizer, &out->args.save);
        break;
    }
    if (err != parse_command__ok) {
        Command_destroy(out);
        return err;
    }

    Token eof_token = Tokenizer_next(tokenizer);
    if (eof_token.tag != TOKEN_EOF) {
        Command_destroy(out);
        return parse_command__expected_eof;
    }
    return parse_command__ok;
}
