#pragma once

#include <stdio.h>
#include <stdlib.h>

#define UNREACHABLE                                                                                \
    do {                                                                                           \
        fprintf(stderr, "Reached unreachable code: %s:%d\n", __FILE__, __LINE__);                  \
        abort();                                                                                   \
    } while (0)
