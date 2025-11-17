// Cross-platform fmemopen implementation

#pragma once

#include <stdio.h> // On non-windows systems, this includes fmemopen
#include <stdlib.h>

#if defined(_WIN32)
// Windows does not have fmemopen, so we fake it by opening a temporary file
// Original fmemopen docs: https://www.man7.org/linux/man-pages/man3/fmemopen.3.html
FILE* fmemopen(void* buf, size_t size, const char* __modes) {
    FILE* fptr;
    tmpfile_s(&fptr);
    fwrite(buf, sizeof(char), size, fptr);
    fseek(fptr, 0, SEEK_SET);
    return fptr;
}
#endif
