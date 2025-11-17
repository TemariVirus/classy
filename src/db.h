#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "error.h"
#include "row.h"
#include "string_helper.h"
#include "t-tree.h"

#define MAX_LINE_LEN 4096
#define LINE_TERM "\r\n"

typedef struct DB {
    // Stores all IDs and rows.
    TTree data;
    // The number of rows in the database.
    size_t row_count;
    // The name of the DB's only table.
    char* table_name;
} DB;

// Free all memory used by the database.
void DB_destroy(DB* db) {
    if (db == NULL) {
        return;
    }

    TTree_destroy(&db->data);
    db->row_count = 0;
    free(db->table_name);
    db->table_name = NULL;
}

static bool __parse_columns(char* line, Column columns[COLUMN_COUNT]) {
    StringSplit split = {.current = line};
    for (int i = 0; i < COLUMN_COUNT; i++) {
        char* col_name = string_split_next(&split, ',');
        if (!Column_from_name(col_name, &columns[i])) {
            return false;
        }
    }

    // Check that all columns were parsed
    const ColumnsMask full_mask = (1 << COLUMN_COUNT) - 1;
    ColumnsMask mask = 0;
    for (int i = 0; i < COLUMN_COUNT; i++) {
        mask |= 1 << columns[i];
    }
    return mask == full_mask;
}

typedef enum {
    DB_from_file__ok = ERROR_OK,
    DB_from_file__missing_table_name = ERROR_MISSING_TABLE_NAME,
    DB_from_file__bad_format = ERROR_BAD_FORMAT,
    DB_from_file__bad_column = ERROR_BAD_COLUMN,
    DB_from_file__unordered_id = ERROR_UNORDERED_ID,
} DB_from_file__Error;

// Creates a new DB at `out_db` and inserts all rows from the file.
// If there was an error, `out_db` is uninitialised.
DB_from_file__Error DB_from_file(FILE* fptr, DB* out_db) {
    assert(fptr != NULL);
    assert(out_db != NULL);

    DB_from_file__Error err;
    TTreeBulkInsert bulk = TTree_bulk_insert_start();
    char* table_name = NULL;
    *out_db = (DB){
        .data = (TTree){.root = NULL, .node_allocator = NULL},
        .row_count = 0,
        .table_name = NULL,
    };

    char line[MAX_LINE_LEN];
    // Ignore all headers
    while (true) {
        if (!read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
            err = DB_from_file__bad_format;
            goto error_cleanup;
        }
        if (str_empty(line)) {
            // Blank line indicates end of headers
            break;
        }
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

    // Parse rows
    ID last_id = 0;
    size_t row_count = 0;
    while (read_until_delim_or_eof(line, MAX_LINE_LEN, LINE_TERM, fptr)) {
        ID id;
        Row row;
        StringSplit split = {.current = line};
        for (int i = 0; i < COLUMN_COUNT; i++) {
            switch (columns[i]) {
            case COLUMN_ID: {
                char* id_str = string_split_next(&split, ',');
                if (!parse_id(id_str, &id)) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                if (row_count > 0 && id <= last_id) {
                    err = DB_from_file__unordered_id;
                    goto error_cleanup;
                }
                last_id = id;
                break;
            }
            case COLUMN_NAME:
            case COLUMN_PROGRAMME: {
                char* str_end = read_string(split.current);
                if (str_end == NULL || (str_end[0] != ',' && str_end[0] != '\0')) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                if (columns[i] == COLUMN_NAME) {
                    row.name = split.current;
                } else {
                    row.programme = split.current;
                }
                // Skip comma
                split.current = &str_end[1];
                break;
            }
            case COLUMN_MARK: {
                char* float_str = string_split_next(&split, ',');
                if (!parse_float(float_str, &row.mark)) {
                    err = DB_from_file__bad_format;
                    goto error_cleanup;
                }
                break;
            }
            }
        }
        TTree_bulk_insert(&bulk, id, &row);
        row_count++;
    }
    // Ensure the last line was properly terminated
    if (!str_empty(line)) {
        err = DB_from_file__bad_format;
        goto error_cleanup;
    }

    *out_db = (DB){
        .data = TTree_bulk_insert_end(&bulk),
        .row_count = row_count,
        .table_name = table_name,
    };
    return DB_from_file__ok;

error_cleanup: {
    TTree tree = TTree_bulk_insert_end(&bulk);
    TTree_destroy(&tree);
    free(table_name);
    return err;
}
}
