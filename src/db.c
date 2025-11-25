// The database structure used by Classy.
// Serialize to a file with DB_to_file and load from a file with DB_from_file.

#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.c"
#include "record.c"
#include "string_helper.c"
#include "t-tree.c"
#include "tokenizer.c"
#include "unreachable.c"

#define TYPE char*
#define TYPED(THING) String##THING
#include "list.c"

#define MAX_LINE_LEN 4096
#define LINE_TERM "\r\n"

typedef struct DB {
    // Stores all IDs and records.
    TTree data;
    // The number of records in the database.
    size_t record_count;
    // The name of the DB's only table.
    char* table_name;
    // Raw headers from the file (currently not used).
    StringList headers;
} DB;

// Create an empty database.
//
// The DB must be destroyed with `DB_destroy` to free memory.
DB DB_create(void) {
    return (DB){
        .data = TTree_create(),
        .record_count = 0,
        .table_name = NULL,
        .headers = StringList_create(),
    };
}

// Free all memory used by the database.
void DB_destroy(DB* db) {
    if (db == NULL) {
        return;
    }

    TTree_destroy(&db->data);
    db->record_count = 0;
    free(db->table_name);
    db->table_name = NULL;

    for (size_t i = 0; i < db->headers.length; i++) {
        free(db->headers.items[i]);
    }
    StringList_destroy(&db->headers);
}

// Parses the column line in the file into an array of Columns.
// Returns true on success, false on failure.
static bool __parse_columns(char* line, Column columns[COLUMN_COUNT]) {
    StringSplit split = {.current = line, .delim = ','};
    for (int i = 0; i < COLUMN_COUNT; i++) {
        char* col_name = string_split_next(&split);
        columns[i] = str_to_column(&col_name);
        if (col_name == NULL || !str_empty(col_name)) {
            return false;
        }
    }

    // Check that all columns were parsed
    ColumnsMask mask = COLUMNS_MASK_EMPTY;
    for (int i = 0; i < COLUMN_COUNT; i++) {
        ColumnsMask_set(&mask, columns[i]);
    }
    return mask == COLUMNS_MASK_FULL;
}

typedef enum {
    DB_from_file__ok = ERROR_OK,
    DB_from_file__missing_table_name = ERROR_MISSING_TABLE_NAME,
    DB_from_file__bad_format = ERROR_BAD_DB_FORMAT,
    DB_from_file__bad_column = ERROR_BAD_DB_COLUMN,
    DB_from_file__unordered_id = ERROR_UNORDERED_ID,
    DB_from_file__out_of_mem = ERROR_OUT_OF_MEM,
} DB_from_file__Error;

// Creates a new DB at `out_db` and inserts all records from the file.
// If there was an error, `out_db` is uninitialised.
DB_from_file__Error DB_from_file(FILE* fptr, DB* out_db) {
    assert(fptr != NULL);
    assert(out_db != NULL);

    DB_from_file__Error err;
    TTreeBulkInsert bulk = TTree_bulk_insert_start();
    char* table_name = NULL;
    StringList headers = StringList_create();
    *out_db = (DB){
        .data = (TTree){.root = NULL, .node_allocator = NULL},
        .record_count = 0,
        .table_name = NULL,
        .headers = StringList_create(),
    };

    char line[MAX_LINE_LEN];
    // Collect headers
    while (true) {
        if (!read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
            err = DB_from_file__bad_format;
            goto error_cleanup;
        }
        if (str_empty(line)) {
            // Blank line indicates end of headers
            break;
        }
        char* duped = strdup(line);
        if (duped == NULL) {
            err = DB_from_file__out_of_mem;
            goto error_cleanup;
        }
        StringList_append(&headers, duped);
    }

    // Read table name
    if (!read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
        err = DB_from_file__bad_format;
        goto error_cleanup;
    }
    if (strncmp(line, "Table Name: ", 12) != 0) {
        err = DB_from_file__missing_table_name;
        goto error_cleanup;
    }
    table_name = strdup(&line[12]);
    if (str_empty(table_name)) {
        // Table name cannot be empty
        err = DB_from_file__missing_table_name;
        goto error_cleanup;
    }

    // Parse columns
    Column columns[COLUMN_COUNT];
    if (!read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
        err = DB_from_file__bad_format;
        goto error_cleanup;
    }
    if (!__parse_columns(line, columns)) {
        err = DB_from_file__bad_column;
        goto error_cleanup;
    }

    // Parse records
    ID last_id = 0;
    size_t record_count = 0;
    while (read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
        ID id;
        Record record;
        StringSplit split = {.current = line, .delim = ','};
        for (int i = 0; i < COLUMN_COUNT; i++) {
            switch (columns[i]) {
            case COLUMN_ID: {
                char* id_str = string_split_next(&split);
                if (!parse_id(id_str, &id)) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                if (record_count > 0 && id <= last_id) {
                    err = DB_from_file__unordered_id;
                    goto error_cleanup;
                }
                last_id = id;
                break;
            }
            case COLUMN_NAME:
            case COLUMN_PROGRAMME: {
                char* str = read_escaped_string(&split.current);
                bool is_next_delimeter = (split.current[0] == ',') || (split.current[0] == '\0');
                if (str == NULL || !is_next_delimeter) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                if (columns[i] == COLUMN_NAME) {
                    record.name = str;
                } else if (columns[i] == COLUMN_PROGRAMME) {
                    record.programme = str;
                } else {
                    UNREACHABLE;
                }
                // Skip comma
                split.current++;
                break;
            }
            case COLUMN_MARK: {
                char* float_str = string_split_next(&split);
                if (!parse_float(float_str, &record.mark)) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                break;
            }
            }
        }
        TTree_bulk_insert(&bulk, id, &record);
        record_count++;
    }
    // Ensure the last line was properly terminated
    if (!str_empty(line)) {
        err = DB_from_file__bad_format;
        goto error_cleanup;
    }

    *out_db = (DB){
        .data = TTree_bulk_insert_end(&bulk),
        .record_count = record_count,
        .table_name = table_name,
        .headers = headers,
    };
    return DB_from_file__ok;

error_cleanup: {
    TTree tree = TTree_bulk_insert_end(&bulk);
    TTree_destroy(&tree);
    free(table_name);
    StringList_destroy(&headers);
    return err;
}
}

// Serialises `db` and all its records to the given file.
// Returns whether the operation was successful.
bool DB_to_file(const DB* db, FILE* fptr) {
    // Headers
    for (size_t i = 0; i < db->headers.length; i++) {
        if (fprintf(fptr, "%s\r\n", db->headers.items[i]) < 0) {
            return false;
        }
    }
    if (fprintf(fptr, "\r\n") < 0) {
        return false;
    }

    // Table name
    if (fprintf(fptr, "Table Name: %s\r\n", db->table_name) < 0) {
        return false;
    }

    // Column headers
    if (fprintf(fptr, "ID,Name,Programme,Mark\r\n") < 0) {
        return false;
    }
    // Iterate through all records
    TTreeIter it = TTree_iter_start(&db->data);
    ID id;
    Record* record;
    while (TTree_iter_next(&it, &id, &record)) {
        if (fprintf(fptr, "%u,", id) < 0) {
            return false;
        }
        if (!escape_string(fptr, record->name)) {
            return false;
        }
        if (fputc(',', fptr) == EOF) {
            return false;
        }
        if (!escape_string(fptr, record->programme)) {
            return false;
        }
        if (fprintf(fptr, ",%.9g\r\n", record->mark) < 0) {
            return false;
        }
    }

    return true;
}
