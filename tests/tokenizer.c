#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <math.h>
#include <string.h>

#include "../src/tokenizer.c"
#include "testing.c"

TEST tokenizer_empty(void) {
    START_TEST("Tokenizer empty");

    {
        char line[] = "";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
        // Run again to test idempotency
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_unkown(void) {
    START_TEST("Tokenizer unknown");

    {
        char line[] = "a";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = "SHOW ALL name";
        Tokenizer tokenizer = Tokenizer_create(line);

        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, token.data.cmd);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_int_and_float(void) {
    START_TEST("Tokenizer int and float");

    {
        char line[] = "123 -123 2.35 2e3 5.43e2 inf 00012 .123";
        Tokenizer tokenizer = Tokenizer_create(line);
        // 123
        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_INT, token.tag);
        EXPECT_INT_EQUAL(123, token.data.ui);
        // -123 (we only support unsigned ints so this is a float)
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)-123, token.data.f);
        // 2.35
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)2.35, token.data.f);
        // 2e3
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)2e3, token.data.f);
        // 5.43e2
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)5.43e2, token.data.f);
        // inf
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)INFINITY, token.data.f);
        // 00012
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_INT, token.tag);
        EXPECT_FLOAT_EQUAL(12, token.data.ui);
        // .123
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)0.123, token.data.f);

        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_string(void) {
    START_TEST("tokenizer string");

    {
        char line[] = R"("abcd")"
                      R"("\"abc\\\"cd\\")"
                      R"("\\\\\\")"
                      R"("\\\"\"\\\\")";
        Tokenizer tokenizer = Tokenizer_create(line);

        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("abcd", token.data.s);

        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("\"abc\\\"cd\\", token.data.s);

        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("\\\\\\", token.data.s);

        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("\\\"\"\\\\", token.data.s);

        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_mixed(void) {
    START_TEST("tokenizer string");

    {
        char line[] = R"(SHOW ALL ID= 2 SORT BY Name DESC (3.<5 "a\"b" IN Programme)QUERY)";
        Tokenizer tokenizer = Tokenizer_create(line);

        // SHOW ALL
        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, token.data.cmd);
        // ID= 2
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_COLUMN, token.tag);
        EXPECT_INT_EQUAL(COLUMN_ID, token.data.col);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_OP, token.tag);
        EXPECT_INT_EQUAL(OP_EQ, token.data.op);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_INT, token.tag);
        EXPECT_INT_EQUAL(2, token.data.ui);
        // SORT BY Name DESC
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_SORT_BY, token.tag);
        EXPECT_INT_EQUAL(COLUMN_NAME, token.data.sort_by.column);
        EXPECT(!token.data.sort_by.ascending);
        // (
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_OP, token.tag);
        EXPECT_INT_EQUAL(OP_LPAREN, token.data.op);
        // 3.<5
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_FLOAT, token.tag);
        EXPECT_FLOAT_EQUAL((float)3, token.data.f);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_OP, token.tag);
        EXPECT_INT_EQUAL(OP_LT, token.data.op);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_INT, token.tag);
        EXPECT_INT_EQUAL(5, token.data.op);
        // "a\"b" IN Programme
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("a\"b", token.data.s);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_OP, token.tag);
        EXPECT_INT_EQUAL(OP_IN, token.data.op);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_COLUMN, token.tag);
        EXPECT_INT_EQUAL(COLUMN_PROGRAMME, token.data.col);
        // )
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_OP, token.tag);
        EXPECT_INT_EQUAL(OP_RPAREN, token.data.op);
        // QUERY
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_QUERY, token.data.cmd);

        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_string_bad_escape(void) {
    START_TEST("tokenizer string");

    {
        char line[] = R"("ac\n dea")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = R"("ac\\\r dea")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = R"(SHOW ALL "ac\" \ dea")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_CMD, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_open_cmd(void) {
    START_TEST("tokenizer open cmd");

    {
        char line[] = R"(OPEN "abcd" QUERY ID=2)";
        Tokenizer tokenizer = Tokenizer_create(line);

        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_OPEN, token.data.cmd);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL(R"("abcd" QUERY ID=2)", token.data.s);

        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = R"(OPEN "abcd \\ \"efg")";
        Tokenizer tokenizer = Tokenizer_create(line);

        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_OPEN, token.data.cmd);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL(R"("abcd \\ \"efg")", token.data.s);

        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = R"(OPEN    surrounded by 3 spaces.txt   )";
        Tokenizer tokenizer = Tokenizer_create(line);

        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_OPEN, token.data.cmd);
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL(R"(   surrounded by 3 spaces.txt   )", token.data.s);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}

TEST tokenizer_missing_spaces(void) {
    START_TEST("tokenizer missing spaces");

    {
        char line[] = "SHOW ALLID = 20";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = "SHOW ALL \"abc\"INName";
        Tokenizer tokenizer = Tokenizer_create(line);
        // SHOW ALL
        Token token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_CMD, token.tag);
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, token.data.cmd);
        // "abc"
        token = Tokenizer_next(&tokenizer);
        EXPECT_INT_EQUAL(TOKEN_STRING, token.tag);
        EXPECT_STRING_EQUAL("abc", token.data.s);
        // INName
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    {
        char line[] = "QUERY(ID=1)";
        Tokenizer tokenizer = Tokenizer_create(line);
        // QUERY(ID=1)
        EXPECT_INT_EQUAL(TOKEN_UNK, Tokenizer_next(&tokenizer).tag);
        EXPECT_INT_EQUAL(TOKEN_EOF, Tokenizer_next(&tokenizer).tag);
    }

    END_TEST();
}
