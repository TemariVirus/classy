#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "error.h"

int main(void) {
    DB db;
    FILE* fptr = fopen("sample-db.txt", "rb");
    if (fptr == NULL) {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }
    DB_from_file__Error err = DB_from_file(fptr, &db);
    fclose(fptr);
    switch (err) {
    case ERROR_OK:
        break;
    case ERROR_MISSING_TABLE_NAME:
        printf("Error: Missing table name in file.\n");
        break;
    case ERROR_BAD_FORMAT:
        printf("Error: Badly formatted file.\n");
        break;
    case ERROR_BAD_COLUMN:
        printf("Error: Column name must be one of ID, Name, Programme, or Mark and cannot contain "
               "duplicates.\n");
        break;
    case ERROR_UNORDERED_ID:
        printf("Error: IDs in file are not in ascending order.\n");
        break;
    }

    printf("Table Name: %s\n", db.table_name);
    printf("Row Count: %zu\n", db.row_count);

    ID id;
    Row* row;
    TTreeIter it = TTree_iter_start(&db.data);
    while (TTree_iter_next(&it, &id, &row)) {
        printf("%10i: name=%s\t programme=%s\t mark=%f\n", id, row->name, row->programme,
               row->mark);
    }

    DB_destroy(&db);

    return 0;
}
