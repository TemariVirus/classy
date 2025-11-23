// Cross-platform fmemopen implementation

#pragma once

#include <stdio.h> // On non-windows systems, this includes fmemopen
#include <stdlib.h>

#if defined(_WIN32)
// Windows does not have fmemopen, so we fake it by opening a temporary file
// Original fmemopen docs: https://www.man7.org/linux/man-pages/man3/fmemopen.3.html
FILE* fmemopen(void* buf, size_t size, const char* __modes) {
    (void)__modes;

    FILE* fptr;
    if (tmpfile_s(&fptr) != 0) {
        return NULL;
    }
    fwrite(buf, 1, size, fptr);
    if (__modes[0] != 'a') {
        // If not appending, rewind to the beginning
        fseek(fptr, 0, SEEK_SET);
    }
    return fptr;
}
#endif
