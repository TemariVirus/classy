#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.c"
#include "row.c"
#include "t-tree.c"

#define LINE_END "\n"

static char* last_save_filename = NULL;

// escape quotes in CSV fields
static void escape_csv_field(const char* input, char* output, size_t size) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j + 1 < size; i++) {
        if (input[i] == '"') {
            if (j + 2 >= size) break;
            output[j++] = '"';
        }
        output[j++] = input[i];
    }
    output[j] = '\0';
}

// save db to filename, save to previous filename if null
bool DB_save(DB* db, const char* filename) {
    if (!db) return false;

    const char* save_filename = filename;
    if (!save_filename) {
        if (!last_save_filename) {
            fprintf(stderr, "No filename specified and no previous save/open file.\n");
            return false;
        }
        save_filename = last_save_filename;
    }

    FILE* fptr = fopen(save_filename, "w");
    if (!fptr) {
        fprintf(stderr, "Cannot open file %s for writing.\n", save_filename);
        return false;
    }

    // preserve database header
    fprintf(fptr, "Database Name: Sample-CMS%s", LINE_END);
    fprintf(fptr, "Authors: Assistant Prof Oran Zane Devilly%s", LINE_END);
    fprintf(fptr, "%s", LINE_END);

    // table name
    fprintf(fptr, "Table Name: %s%s", db->table_name, LINE_END);

    // column headers
    fprintf(fptr, "ID,Name,Programme,Mark%s", LINE_END);

    // iteration through all records
    TTreeIter iter = TTree_iter_start(&db->data);
    ID id;
    Row* row;
    while (TTree_iter_next(&iter, &id, &row)) {
        char name_buf[1024], prog_buf[1024];
        escape_csv_field(row->name, name_buf, sizeof(name_buf));
        escape_csv_field(row->programme, prog_buf, sizeof(prog_buf));

        fprintf(fptr, "%u,\"%s\",\"%s\",%.1f%s",
            id,
            name_buf,
            prog_buf,
            row->mark,
            LINE_END
        );
    }

    fclose(fptr);

    // store last used filename
    if (filename) {
        free(last_save_filename);
        last_save_filename = strdup(filename);
    }

    return true;
}
