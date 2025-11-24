#pragma once

#include <math.h>
#include <stdio.h>

#include "../src/db.c"
#include "../src/fmemopen.c"
#include "testing.c"

TEST db_from_file_missing_table_name(void) {
    START_TEST("DB from file missing table name");

    char db_file[] = "\r\n"
                     "Table Name: \r\n";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");
    DB db;
    EXPECT_INT_EQUAL(ERROR_MISSING_TABLE_NAME, DB_from_file(fptr, &db));

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}

TEST db_from_file_bad_format(void) {
    START_TEST("DB from file bad format");

    char db_file[] = "\r\n"
                     "Table Name: abc\r\n"
                     "ID,Mark,Name,Programme\r\n"
                     R"(1234,64.4,"name",programme")"
                     "\r\n";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");
    DB db;
    EXPECT_INT_EQUAL(ERROR_BAD_DB_FORMAT, DB_from_file(fptr, &db));

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}

TEST db_from_file_unordered_id(void) {
    START_TEST("DB from file unordered ID");

    char db_file[] = "\r\n"
                     "Table Name: abc\r\n"
                     "ID,Mark,Name,Programme\r\n"
                     R"(4321,64.4,"name","programme")"
                     "\r\n"
                     R"(1234,64.4,"name","programme")"
                     "\r\n";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");
    DB db;
    EXPECT_INT_EQUAL(ERROR_UNORDERED_ID, DB_from_file(fptr, &db));

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}

TEST db_from_file_missing_line_term(void) {
    START_TEST("DB from file missing line terminator");

    char db_file[] = "\r\n"
                     "Table Name: abc\r\n"
                     "ID,Mark,Name,Programme\r\n"
                     R"(1234,64.4,"name","programme")";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");
    DB db;
    EXPECT_INT_EQUAL(ERROR_BAD_DB_FORMAT, DB_from_file(fptr, &db));

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}

TEST db_from_file_successful(void) {
    START_TEST("DB from file successful");

    char db_file[] = "\r\n"
                     "Table Name: abc\r\n"
                     "Name,ID,Mark,Programme\r\n"
                     R"("student a",0,12.3,"programme a")"
                     "\r\n"
                     R"("student b",123,4.56,"programme a")"
                     "\r\n"
                     R"("student c",321,-2e22,"math \"\"\\\\ \"escape me\"")"
                     "\r\n"
                     R"("student d",42069,inf,"limits \\n calculus")"
                     "\r\n";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");

    DB db;
    EXPECT_INT_EQUAL(ERROR_OK, DB_from_file(fptr, &db));
    EXPECT_STRING_EQUAL("abc", db.table_name);
    EXPECT_INT_EQUAL(4, db.record_count);

    ID id;
    Record* record;
    TTreeIter it = TTree_iter_start(&db.data);
    EXPECT(TTree_iter_next(&it, &id, &record));
    EXPECT_STRING_EQUAL("student a", record->name);
    EXPECT_INT_EQUAL(0, id);
    EXPECT_FLOAT_EQUAL((float)12.3, record->mark);
    EXPECT_STRING_EQUAL("programme a", record->programme);

    EXPECT(TTree_iter_next(&it, &id, &record));
    EXPECT_STRING_EQUAL("student b", record->name);
    EXPECT_INT_EQUAL(123, id);
    EXPECT_FLOAT_EQUAL((float)4.56, record->mark);
    EXPECT_STRING_EQUAL("programme a", record->programme);

    EXPECT(TTree_iter_next(&it, &id, &record));
    EXPECT_STRING_EQUAL("student c", record->name);
    EXPECT_INT_EQUAL(321, id);
    EXPECT_FLOAT_EQUAL((float)-2e22, record->mark);
    EXPECT_STRING_EQUAL("math \"\"\\\\ \"escape me\"", record->programme);

    EXPECT(TTree_iter_next(&it, &id, &record));
    EXPECT_STRING_EQUAL("student d", record->name);
    EXPECT_INT_EQUAL(42069, id);
    EXPECT_FLOAT_EQUAL((float)INFINITY, record->mark);
    EXPECT_STRING_EQUAL("limits \\n calculus", record->programme);

    EXPECT(!TTree_iter_next(&it, &id, &record));

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}

TEST db_from_file_empty(void) {
    START_TEST("DB from file empty");

    char db_file[] = "\r\n"
                     "Table Name: a\r\n"
                     "Name,ID,Mark,Programme\r\n";
    FILE* fptr = fmemopen(db_file, sizeof(db_file), "rb");

    DB db;
    EXPECT_INT_EQUAL(ERROR_OK, DB_from_file(fptr, &db));
    EXPECT_STRING_EQUAL("a", db.table_name);
    EXPECT_INT_EQUAL(0, db.record_count);

    DB_destroy(&db);
    fclose(fptr);
    END_TEST();
}
