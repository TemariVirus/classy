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
// We'll start ours at 1000 to avoid clashes.
#define ERROR_EXAMPLE_REMOVE_ME_LATER 1001
