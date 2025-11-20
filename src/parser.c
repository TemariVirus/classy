#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "tokenizer.h"

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

static bool __parse_expression(Tokenizer*, uint8_t, Expression*);

ExpressionTag __column_type(Column column) {
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
bool prefix_binding_power(OpTag op, uint8_t* rbp) {
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
bool infix_binding_power(OpTag op, uint8_t* lbp, uint8_t* rbp) {
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
    assert(false); // unreachable
}

// Recursively free the memory used by a condition.
void condition_destroy(Condition* cond) {
    if (cond == NULL) {
        return;
    }
    switch (cond->tag) {
    case OP_NOT:
        condition_destroy(cond->args.not.cond);
        break;
    case OP_AND:
    case OP_OR:
        condition_destroy(cond->args.and_or.lhs);
        condition_destroy(cond->args.and_or.rhs);
        break;
    default:
        break;
    }
}

// Recursively free the memory used by an expression.
static void __expression_destroy(Expression* expr) {
    if (expr == NULL) {
        return;
    }
    switch (expr->tag) {
    case EXPR_CONDITION:
        condition_destroy(expr->value.cond);
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
        assert(false); // Unreachable
    }
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

// Recursive Pratt parser for expressions.
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
            if (!prefix_binding_power(token.data.op, &rbp)) {
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
            !infix_binding_power(op_token.data.op, &lbp, &rbp)) {
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
// The returned condition must be freed with `condition_destroy`.
Condition* parse_condition(Tokenizer* tokenizer) {
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
