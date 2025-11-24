#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "record.c"
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

// A condition used for filtering records.
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

// Returns whether the given ID and Record fulfill `cond`.
// Returns true if `cond` is NULL.
bool Condition_eval(const Condition* cond, ID id, Record* record) {
    if (cond == NULL) {
        return true;
    }

    record->temp_id = id; // Needed for `Record_get_int` to work
    switch (cond->tag) {
    case OP_OR:
        return Condition_eval(cond->args.and_or.lhs, id, record) ||
               Condition_eval(cond->args.and_or.rhs, id, record);
    case OP_AND:
        return Condition_eval(cond->args.and_or.lhs, id, record) &&
               Condition_eval(cond->args.and_or.rhs, id, record);
    case OP_NOT:
        return !Condition_eval(cond->args.not.cond, id, record);
    case OP_IN:
        return strstr(Record_get_string(record, cond->args.in.column), cond->args.in.value) != NULL;
    case OP_LT:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return Record_get_int(record, cond->args.eq_gt_lt.column) <
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return Record_get_float(record, cond->args.eq_gt_lt.column) <
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            UNREACHABLE;
        }
    case OP_GT:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return Record_get_int(record, cond->args.eq_gt_lt.column) >
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return Record_get_float(record, cond->args.eq_gt_lt.column) >
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            UNREACHABLE;
        }
    case OP_EQ:
        switch (Column_type(cond->args.eq_gt_lt.column)) {
        case VALUE_INT:
            return Record_get_int(record, cond->args.eq_gt_lt.column) ==
                   cond->args.eq_gt_lt.value.ui;
        case VALUE_FLOAT:
            return Record_get_float(record, cond->args.eq_gt_lt.column) ==
                   cond->args.eq_gt_lt.value.f;
        case VALUE_STRING:
            return strcmp(Record_get_string(record, cond->args.eq_gt_lt.column),
                          cond->args.eq_gt_lt.value.s) == 0;
        }
    case OP_RPAREN:
    case OP_LPAREN:
        UNREACHABLE;
    }
}
