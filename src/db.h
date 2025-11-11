#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "t-tree.h"

typedef struct {
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
    free(db->table_name);
}
