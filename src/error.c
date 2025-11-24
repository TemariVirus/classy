// Classy's global error set.
//
// More fine-grained error sets can be defined via enums that use the values
// in this file. This allows functions to return more specific error codes and
// for us to exhaustively handle them in switch statements.
//
// Functions should refrain from returning `ClassyError` and instead return more
// specific error types when possible.

#pragma once

// Any error may be cast to this type.
// On most compilers, enums are backed by the `int` type.
typedef int ClassyError;

#define ERROR_OK 0
// <errno.h> uses 1-131 for its error codes.
// We'll start ours at 1001 to avoid clashes.
#define ERROR_CANNOT_ACCESS_FILE 1001
#define ERROR_MISSING_TABLE_NAME 1002
#define ERROR_BAD_DB_FORMAT 1003
#define ERROR_BAD_DB_COLUMN 1004
#define ERROR_UNORDERED_ID 1005
#define ERROR_EXPECTED_EOF 1006
#define ERROR_EXPECTED_COMMAND 1007
#define ERROR_EXPECTED_VALUE 1008
#define ERROR_EXPECTED_INT 1009
#define ERROR_EXPECTED_FLOAT 1010
#define ERROR_EXPECTED_STR 1011
#define ERROR_EXPECTED_COL 1012
#define ERROR_EXPECTED_ID_COL 1013
#define ERROR_EXPECTED_STR_COL 1014
#define ERROR_EXPECTED_OP_EQ 1015
#define ERROR_EXPECTED_COND 1016
#define ERROR_DUPLICATE_COL 1017
#define ERROR_MISMATCHED_PAREN 1018
#define ERROR_BAD_OP 1019
#define ERROR_OUT_OF_MEM 1020
