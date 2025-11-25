#pragma once

#include <stdio.h>
#include <stdlib.h>

// It is a logic error for this macro to be run.
// Only use this is places that should be unreachable under any circumstances.
// This is not a recoverable error or a panic, it indicates a bug in the program.
#define UNREACHABLE                                                                                \
    do {                                                                                           \
        fprintf(stderr, "Reached unreachable code: %s:%d\n", __FILE__, __LINE__);                  \
        abort();                                                                                   \
    } while (0)
