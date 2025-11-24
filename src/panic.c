#pragma once

#include <stdio.h>
#include <stdlib.h>

#define PANIC(message)                                                                             \
    do {                                                                                           \
        const char* reason = message; /* For type safety */                                        \
        fprintf(stderr,                                                                            \
                "Panic at: %s:%d\n"                                                                \
                "Reason: %s",                                                                      \
                __FILE__, __LINE__, reason);                                                       \
        abort();                                                                                   \
    } while (0)
