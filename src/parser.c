#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "tokenizer.h"
#include "unreachable.c"

struct Condition;
// Strings in this union are references to the original unparsed string, not copies.
typedef union {
    struct {
        // The active field depends on the column.
        union EqGtLtValue {
            uint32_t ui;
            float f;
            char* s;
        } value;
        // Any column.
        Column column;
    } eq_gt_lt;
    struct {
        char* value;
        // Any string column.
        Column column;
    } in;
    struct {
        struct Condition* cond;
    } not;
    struct {
        struct Condition* lhs;
        struct Condition* rhs;
    } and_or;
} ConditionArgs;

// A condition used for filtering rows.
typedef struct Condition {
    ConditionArgs args;
    OpTag tag;
} Condition;

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

// A command and the arguments needed to execute it.
typedef struct {
    // Strings in this union are references to the original unparsed string, not copies.
    union {
        struct CmdOpenArgs {
            // Name of the file to open. Cannot be NULL.
            char* filename;
        } open;
        struct CmdSaveArgs {
            // Name of the file to save to. Can be NULL.
            char* filename;
        } save;
        struct CmdShowAllArgs {
            // How to sort the rows.
            SortBy sort_by;
        } show_all;
        struct CmdSummaryArgs {
            // Condition to filter rows. If NULL, no filtering is applied.
            Condition* filter;
        } show_summary;
        struct CmdQueryArgs {
            // Condition to filter rows. Cannot be NULL.
            Condition* filter;
            SortBy sort_by;
        } query;
        struct CmdInsertArgs {
            // The row to insert.
            Row row;
            // The ID of the new row.
            ID id;
        } insert;
        struct CmdUpdateArgs {
            // The row with the updated values.
            Row row;
            // The ID of the row to update.
            ID id;
            // Columns that should be updated have their bit set to 1.
            ColumnsMask update_columns;
        } update;
        struct CmdDeleteArgs {
            // The ID of the row to delete.
            ID id;
        } delete;
    } args;
    CommandTag tag;
} Command;

// The default sort by arguments used when none are specified.
const SortBy DEFAULT_SORT_BY = {
    .column = COLUMN_ID,
    .ascending = true,
};

static bool __parse_expression(Tokenizer*, uint8_t, Expression*);

// The data type of a column.
static ExpressionTag __column_type(Column column) {
    switch (column) {
    case COLUMN_ID:
        return EXPR_INT;
    case COLUMN_MARK:
        return EXPR_FLOAT;
    case COLUMN_NAME:
    case COLUMN_PROGRAMME:
        return EXPR_STRING;
    }
}

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
        switch (__column_type(lhs.value.column)) {
        case EXPR_INT:
        case EXPR_FLOAT:
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
// Returns whether the coercion was successful.
static bool __coerce_eq_gt_lt_value(Column lhs_column, Expression rhs,
                                    union EqGtLtValue* out_value) {
    switch (__column_type(lhs_column)) {
    case EXPR_INT:
        switch (rhs.tag) {
        case EXPR_INT:
            out_value->ui = rhs.value.ui;
            return true;
        default:
            return false;
        }
    case EXPR_FLOAT:
        switch (rhs.tag) {
        case EXPR_INT:
            out_value->f = rhs.value.ui;
            return true;
        case EXPR_FLOAT:
            out_value->f = rhs.value.f;
            return true;
        default:
            return false;
        }
    case EXPR_STRING:
        switch (rhs.tag) {
        case EXPR_STRING:
            out_value->s = rhs.value.s;
            return true;
        default:
            return false;
        }
    default:
        break;
    }

    UNREACHABLE;
}

// Recursively free the memory used by a condition.
static void __condition_destroy(Condition* cond) {
    if (cond == NULL) {
        return;
    }
    switch (cond->tag) {
    case OP_NOT:
        __condition_destroy(cond->args.not.cond);
        break;
    case OP_AND:
    case OP_OR:
        __condition_destroy(cond->args.and_or.lhs);
        __condition_destroy(cond->args.and_or.rhs);
        break;
    default:
        break;
    }
}

// Free all memory used by an expression.
static void __expression_destroy(Expression* expr) {
    if (expr == NULL) {
        return;
    }
    switch (expr->tag) {
    case EXPR_CONDITION:
        __condition_destroy(expr->value.cond);
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
        __condition_destroy(cmd->args.show_summary.filter);
        break;
    case CMD_QUERY:
        __condition_destroy(cmd->args.query.filter);
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
        break;
    }

    UNREACHABLE;
}

// Create a condition at `out_expr` with the prefix operator `op`,
// right binding power `rbp`, and operand parsed from `tokenizer`.
// Returns whether the condition was successfully created.
static bool __prefixed_condition(OpTag op, uint8_t rbp, Tokenizer* tokenizer,
                                 Expression* out_expr) {
    Condition* cond = NULL;
    Expression rhs;
    bool also_free_rhs = false;

    // Get operand
    if (!__parse_expression(tokenizer, rbp, &rhs)) {
        goto error_cleanup;
    }
    also_free_rhs = true;

    // Construct condition with prefix operator
    cond = malloc(sizeof(Condition));
    if (cond == NULL) {
        goto error_cleanup;
    }
    cond->tag = op;
    switch (op) {
    case OP_NOT:
        if (rhs.tag != EXPR_CONDITION) {
            goto error_cleanup;
        }
        cond->args.not.cond = rhs.value.cond;
        also_free_rhs = false;
        break;
    default:
        assert(false); // unreachable
    }

    // Wrap condition in expression
    *out_expr = (Expression){
        .tag = EXPR_CONDITION,
        .value.cond = cond,
    };
    return true;

error_cleanup:
    free(cond);
    if (also_free_rhs) {
        __expression_destroy(&rhs);
    }
    return false;
}

// Create a condition at `out_expr` with the left operand `lhs`, infix operator `op`,
// right binding power `rbp`, and right operand parsed from `tokenizer`.
// Returns whether the condition was successfully created.
static bool __infixed_condition(Expression lhs, OpTag op, uint8_t rbp, Tokenizer* tokenizer,
                                Expression* out_expr) {
    Condition* cond = NULL;
    Expression rhs;
    bool also_free_rhs = false;

    // Get second operand (lhs is the first operand)
    if (!__parse_expression(tokenizer, rbp, &rhs)) {
        goto error_cleanup;
    }
    also_free_rhs = true;

    // Construct condition with infix operator
    cond = malloc(sizeof(Condition));
    if (cond == NULL) {
        goto error_cleanup;
    }
    cond->tag = op;
    switch (op) {
    case OP_EQ:
    case OP_GT:
    case OP_LT:
        if (lhs.tag != EXPR_COLUMN) {
            goto error_cleanup;
        }
        cond->args.eq_gt_lt.column = lhs.value.column;
        if (!__coerce_eq_gt_lt_value(lhs.value.column, rhs, &cond->args.eq_gt_lt.value)) {
            goto error_cleanup;
        }
        break;
    case OP_IN:
        if (rhs.tag != EXPR_COLUMN || __column_type(rhs.value.column) != EXPR_STRING) {
            goto error_cleanup;
        }
        cond->args.in.column = rhs.value.column;
        if (lhs.tag != EXPR_STRING) {
            goto error_cleanup;
        }
        cond->args.in.value = lhs.value.s;
        break;
    case OP_AND:
    case OP_OR:
        if (rhs.tag != EXPR_CONDITION) {
            goto error_cleanup;
        }
        cond->args.and_or.rhs = rhs.value.cond;
        also_free_rhs = false;
        if (lhs.tag != EXPR_CONDITION) {
            goto error_cleanup;
        }
        cond->args.and_or.lhs = lhs.value.cond;
        break;
    default:
        assert(false); // unreachable
    }

    // Wrap condition in expression
    *out_expr = (Expression){
        .tag = EXPR_CONDITION,
        .value.cond = cond,
    };
    return true;

error_cleanup:
    free(cond);
    if (also_free_rhs) {
        __expression_destroy(&rhs);
    }
    return false;
}

// Pratt parser for expressions.
// Parses an expression into `out_expr` from `tokenizer` until the next operator
// with left binding power less than `min_bp` is encountered.
// Returns whether an expression was successfully parsed.
//
// See: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
static bool __parse_expression(Tokenizer* tokenizer, uint8_t min_bp, Expression* out_expr) {
    Expression lhs;

    // Initialise lhs
    Token token = Tokenizer_next(tokenizer);
    switch (token.tag) {
    case TOKEN_EOF:
    case TOKEN_UNK:
    case TOKEN_CMD:
    case TOKEN_SORT_BY:
        // These tokens cannot be part of an expression
        return false;
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
            if (!__parse_expression(tokenizer, 0, &lhs)) {
                return false;
            }
            // Must be closed by right parenthesis
            Token next = Tokenizer_next(tokenizer);
            if (next.tag != TOKEN_OP || next.data.op != OP_RPAREN) {
                goto error_cleanup;
            }
            break;
        default: {
            // Prefix operator
            uint8_t rbp;
            if (!__prefix_binding_power(token.data.op, &rbp)) {
                // Operation cannot be used as prefix
                return false;
            }
            if (!__prefixed_condition(token.data.op, rbp, tokenizer, &lhs)) {
                return false;
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
        if (!__infixed_condition(lhs, op_token.data.op, rbp, tokenizer, &lhs)) {
            goto error_cleanup;
        }
    }

    *out_expr = lhs;
    return true;

error_cleanup:
    __expression_destroy(&lhs);
    return false;
}

// Consumes tokens from `tokenizer` to parse a condition.
// Returns the parsed condition, or NULL on error.
//
// The returned condition must be freed with `__condition_destroy`.
static Condition* __parse_condition(Tokenizer* tokenizer) {
    Expression expr = (Expression){0};
    if (!__parse_expression(tokenizer, 0, &expr)) {
        return NULL;
    }
    switch (expr.tag) {
    case EXPR_CONDITION:
        return expr.value.cond;
    default:
        return NULL;
    }
}

// Parses at most `n` column-value pairs of the form `Column=value`
// from `tokenizer` into `out_id` and `out_row`.
// It is an error for the same column to appear multiple times.
// Returns a bitmask of the seen columns, or 0 on error.
static ColumnsMask __parse_column_values(Tokenizer* tokenizer, int n, ID* out_id, Row* out_row) {
    ColumnsMask seen_columns = 0;
    for (int i = 0; i < n; i++) {
        Token column_token = Tokenizer_next(tokenizer);
        if (column_token.tag != TOKEN_COLUMN) {
            break;
        }
        Column column = column_token.data.col;
        ColumnsMask column_bit = 1 << column;
        if (seen_columns & column_bit) {
            // Column specified multiple times
            return 0;
        }
        seen_columns |= column_bit;

        // '=' must separate column and value
        Token eq_token = Tokenizer_next(tokenizer);
        if (eq_token.tag != TOKEN_OP || eq_token.data.op != OP_EQ) {
            return 0;
        }

        Token value_token = Tokenizer_next(tokenizer);
        switch (value_token.tag) {
        case TOKEN_STRING:
        case TOKEN_INT:
        case TOKEN_FLOAT:
            break;
        default:
            return 0;
        }

        union EqGtLtValue value;
        if (!__coerce_eq_gt_lt_value(column, __expression_from_token(value_token), &value)) {
            return 0;
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
    return seen_columns;
}

// Parses the arguments for the OPEN command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_open(Tokenizer* tokenizer, struct CmdOpenArgs* out) {
    Token filename_token = Tokenizer_next(tokenizer);
    if (filename_token.tag != TOKEN_STRING) {
        return false;
    }
    assert(filename_token.data.s != NULL);
    out->filename = filename_token.data.s;
    return true;
}

// Parses the arguments for the SHOW ALL command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_show_all(Tokenizer* tokenizer, struct CmdShowAllArgs* out) {
    Token sort_by_token = Tokenizer_next(tokenizer);
    switch (sort_by_token.tag) {
    case TOKEN_EOF:
        // No sort by specified, use default
        out->sort_by = DEFAULT_SORT_BY;
        return true;
    case TOKEN_SORT_BY:
        out->sort_by = sort_by_token.data.sort_by;
        return true;
    default:
        break;
    }
    return false;
}

// Parses the arguments for the SHOW SUMMARY command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_show_summary(Tokenizer* tokenizer, struct CmdSummaryArgs* out) {
    if (Tokenizer_peek(tokenizer).tag == TOKEN_EOF) {
        // No condition specified
        out->filter = NULL;
        return true;
    }
    out->filter = __parse_condition(tokenizer);
    return out->filter != NULL;
}

// Parses the arguments for the INSERT command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_insert(Tokenizer* tokenizer, struct CmdInsertArgs* out) {
    ColumnsMask seen_columns = __parse_column_values(tokenizer, COLUMN_COUNT, &out->id, &out->row);
    return seen_columns == ALL_COLUMNS_MASK;
}

// Parses the arguments for the QUERY command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_query(Tokenizer* tokenizer, struct CmdQueryArgs* out) {
    out->filter = __parse_condition(tokenizer);
    if (out->filter == NULL) {
        return false;
    }

    Token sort_by_token = Tokenizer_next(tokenizer);
    switch (sort_by_token.tag) {
    case TOKEN_EOF:
        // No sort by specified, use default
        out->sort_by = DEFAULT_SORT_BY;
        return true;
    case TOKEN_SORT_BY:
        out->sort_by = sort_by_token.data.sort_by;
        return true;
    default:
        break;
    }
    return false;
}

// Parses the arguments for the UPDATE command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_update(Tokenizer* tokenizer, struct CmdUpdateArgs* out) {
    ColumnsMask seen_columns = __parse_column_values(tokenizer, COLUMN_COUNT, &out->id, &out->row);
    // ID column is required
    if ((seen_columns & (1 << COLUMN_ID)) == 0) {
        return false;
    }
    // Set all seen columns except ID
    out->update_columns = seen_columns & ~((ColumnsMask)(1 << COLUMN_ID));
    // At least one column must be updated
    return out->update_columns != 0;
}

// Parses the arguments for the DELETE command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_delete(Tokenizer* tokenizer, struct CmdDeleteArgs* out) {
    Token id_col_token = Tokenizer_next(tokenizer);
    if (id_col_token.tag != TOKEN_COLUMN || id_col_token.data.col != COLUMN_ID) {
        return false;
    }
    Token eq_token = Tokenizer_next(tokenizer);
    if (eq_token.tag != TOKEN_OP || eq_token.data.op != OP_EQ) {
        return false;
    }
    Token id_val_token = Tokenizer_next(tokenizer);
    if (id_val_token.tag != TOKEN_INT) {
        return false;
    }
    out->id = id_val_token.data.ui;
    return true;
}

// Parses the arguments for the SAVE command from `tokenizer` into `out`.
// Returns whether parsing was successful.
static bool __parse_save(Tokenizer* tokenizer, struct CmdSaveArgs* out) {
    Token filename_token = Tokenizer_next(tokenizer);
    switch (filename_token.tag) {
    case TOKEN_EOF:
        // No filename specified
        out->filename = NULL;
        return true;
    case TOKEN_STRING:
        assert(filename_token.data.s != NULL);
        out->filename = filename_token.data.s;
        return true;
    default:
        break;
    }
    return false;
}

// Consumes all tokens from `tokenizer` to parse a command.
// The parsed command is written to `out`.
// Returns whether parsing was successful. Parsing is unsuccessful if there are
// any tokens remaining after the command is parsed.
//
// The returned command must be freed with `Command_destroy`.
bool parse_command(Tokenizer* tokenizer, Command* out) {
    bool success;
    Token cmd_token = Tokenizer_next(tokenizer);
    if (cmd_token.tag != TOKEN_CMD) {
        return false;
    }

    out->tag = cmd_token.data.cmd;
    switch (cmd_token.data.cmd) {
    case CMD_HELP:
        success = true;
        break;
    case CMD_OPEN:
        success = __parse_open(tokenizer, &out->args.open);
        break;
    case CMD_SHOW_ALL:
        success = __parse_show_all(tokenizer, &out->args.show_all);
        break;
    case CMD_SHOW_SUMMARY:
        success = __parse_show_summary(tokenizer, &out->args.show_summary);
        break;
    case CMD_INSERT:
        success = __parse_insert(tokenizer, &out->args.insert);
        break;
    case CMD_QUERY:
        success = __parse_query(tokenizer, &out->args.query);
        break;
    case CMD_UPDATE:
        success = __parse_update(tokenizer, &out->args.update);
        break;
    case CMD_DELETE:
        success = __parse_delete(tokenizer, &out->args.delete);
        break;
    case CMD_SAVE:
        success = __parse_save(tokenizer, &out->args.save);
        break;
    }

    Token eof_token = Tokenizer_next(tokenizer);
    if (!success || eof_token.tag != TOKEN_EOF) {
        Command_destroy(out);
        return false;
    }
    return true;
}
