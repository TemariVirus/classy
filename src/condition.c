#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "row.c"
#include "tokenizer.c"

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

// Recursively free the memory used by a condition.
void Condition_destroy(Condition* cond) {
    if (cond == NULL) {
        return;
    }
    switch (cond->tag) {
    case OP_NOT:
        Condition_destroy(cond->args.not.cond);
        free(cond->args.not.cond);
        break;
    case OP_AND:
    case OP_OR:
        Condition_destroy(cond->args.and_or.lhs);
        free(cond->args.and_or.lhs);
        Condition_destroy(cond->args.and_or.rhs);
        free(cond->args.and_or.rhs);
        break;
    default:
        break;
    }
}

// Returns whether the given ID and Row fulfill `cond`.
// Returns true if `cond` is NULL.
bool Condition_eval(const Condition* cond, ID id, const Row* row) {
    if (cond == NULL) {
        return true;
    }

    // TODO
    (void)id;
    (void)row;
    return true;
}
