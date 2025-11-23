#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "row.c"
#include "tokenizer.c"
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

// Returns the value of the specified column of the given row.
// The data type of `column` must be VALUE_INT.
static uint32_t __get_int_column(ID id, const Row* row, Column column) {
    (void)row;
    switch (column) {
    case COLUMN_ID:
        return id;
    case COLUMN_NAME:
    case COLUMN_PROGRAMME:
    case COLUMN_MARK:
        UNREACHABLE;
    }
}

// Returns the value of the specified column of the given row.
// The data type of `column` must be VALUE_FLOAT.
static float __get_float_column(const Row* row, Column column) {
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
static char* __get_string_column(const Row* row, Column column) {
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

// Returns whether the given ID and Row fulfill `cond`.
// Returns true if `cond` is NULL.
bool Condition_eval(const Condition* cond, ID id, const Row* row) {
    if (cond == NULL) {
        return true;
    }

    switch (cond->tag) {
    case OP_OR:
        return Condition_eval(cond->args.and_or.lhs, id, row) ||
               Condition_eval(cond->args.and_or.rhs, id, row);
    case OP_AND:
        return Condition_eval(cond->args.and_or.lhs, id, row) &&
               Condition_eval(cond->args.and_or.rhs, id, row);
    case OP_NOT:
        return !Condition_eval(cond->args.not.cond, id, row);
    case OP_IN:
        return strstr(__get_string_column(row, cond->args.in.column), cond->args.in.value) != NULL;
    case OP_LT:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return __get_int_column(id, row, cond->args.eq_gt_lt.column) <
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return __get_float_column(row, cond->args.eq_gt_lt.column) <
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            UNREACHABLE;
        }
    case OP_GT:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return __get_int_column(id, row, cond->args.eq_gt_lt.column) >
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return __get_float_column(row, cond->args.eq_gt_lt.column) >
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            UNREACHABLE;
        }
    case OP_EQ:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return __get_int_column(id, row, cond->args.eq_gt_lt.column) ==
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return __get_float_column(row, cond->args.eq_gt_lt.column) ==
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            return strcmp(__get_string_column(row, cond->args.eq_gt_lt.column),
                          cond->args.eq_gt_lt.value.s) == 0;
        }
    case OP_RPAREN:
    case OP_LPAREN:
        UNREACHABLE;
    }
}
