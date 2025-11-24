#pragma once

#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#define MAX_PATH 256
#endif

typedef struct {
    // The pointer to the temporary file. Use this to read/write data.
    FILE* fptr;
    // The name of the temporary file.
    char temp_filename[MAX_PATH];
} AtomicFile;

// Opens a new atomic file in "wb+" mode.
// Returns NULL on failure.
//
// The returned AtomicFile must be closed with AtomicFile_close() or
// AtomicFile_delete() to free its resources.
AtomicFile* AtomicFile_open(const char* mode) {
    AtomicFile* file = malloc(sizeof(AtomicFile));
    if (file == NULL) {
        return NULL;
    }

    // Create temporary file with unique name
#if defined(_WIN32)
    char temp_path[MAX_PATH];
    // Get temp directory path
    if (GetTempPathA(MAX_PATH, temp_path) == 0) {
        goto error_cleanup;
    }
    // Get temp file name and create the file
    if (GetTempFileNameA(temp_path, "classy", 0, file->temp_filename) == 0) {
        goto error_cleanup;
    }
    // Open the temp file
    file->fptr = fopen(file->temp_filename, mode);
    if (file->fptr == NULL) {
        DeleteFileA(file->temp_filename);
        goto error_cleanup;
    }
#else
    // Copy template to temp_filename
    snprintf(file->temp_filename, MAX_PATH, "classy.tmp.XXXXXX");
    int fd = mkstemp(file->temp_filename);
    if (fd == -1) {
        goto error_cleanup;
    }
    // Promote file descriptor to FILE*
    file->fptr = fdopen(fd, mode);
    if (file->fptr == NULL) {
        close(fd);
        unlink(file->temp_filename);
        goto error_cleanup;
    }
#endif

    return file;

error_cleanup:
    free(file);
    return NULL;
}

// Deletes the atomic file and frees its resources.
void AtomicFile_delete(AtomicFile* file) {
    if (file == NULL) {
        return;
    }

    fclose(file->fptr);
#if defined(_WIN32)
    DeleteFileA(file->temp_filename);
#else
    unlink(file->temp_filename);
#endif
    free(file);
}

// Closes the atomic file and moves it to `filename`, freeing `file` on success.
// Returns whether the operation was successful.
bool AtomicFile_close(AtomicFile* file, const char* filename) {
    if (file == NULL || filename == NULL) {
        return false;
    }

    fclose(file->fptr);
#if defined(_WIN32)
    // MoveFile fails if `filename` already exists,
    // while ReplaceFile fails if `filename` does not yet exist.
    // We need to try both to get the overwrite-or-create behaviour of rename().
    // Sigh, Windows...
    if (MoveFile(file->temp_filename, filename) == 0 &&
        ReplaceFile(filename, file->temp_filename, NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_WRITE_THROUGH, NULL, NULL) == 0) {
        return false;
    }
#else
    if (rename(file->temp_filename, filename) == -1) {
        return false;
    }
#endif

    free(file);
    return true;
}
