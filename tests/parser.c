#pragma once

#define __STDC_WANT_LIB_EXT2__ 1
#include <string.h>

#include "../src/parser.c"
#include "../src/tokenizer.c"
#include "testing.c"

TEST parser_help(void) {
    START_TEST("Parser HELP");
    Command command;

    {
        char line[] = "HELP";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_HELP, command.tag);
        Command_destroy(&command);
    }

    {
        char line[] = "HELP Name";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    END_TEST();
}

TEST parser_open(void) {
    START_TEST("Parser OPEN");
    Command command;

    {
        char line[] = "OPEN ";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(OPEN    "surrounded by 3 \\\"spaces.txt"   )";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_OPEN, command.tag);
        EXPECT_STRING_EQUAL(R"(   "surrounded by 3 \\\"spaces.txt"   )",
                            command.args.open.filename);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_show_all(void) {
    START_TEST("Parser SHOW ALL");
    Command command;

    {
        char line[] = "SHOW ALL";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, command.tag);
        EXPECT_INT_EQUAL(COLUMN_ID, command.args.show_all.sort_by.column);
        EXPECT(command.args.show_all.sort_by.ascending);
        Command_destroy(&command);
    }

    {
        char line[] = "SHOW ALL SORT BY";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "SHOW ALL SORT BY Mark";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, command.tag);
        EXPECT_INT_EQUAL(COLUMN_MARK, command.args.show_all.sort_by.column);
        EXPECT(command.args.show_all.sort_by.ascending);
        Command_destroy(&command);
    }

    {
        char line[] = "SHOW ALL SORT BY Name DESC";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_ALL, command.tag);
        EXPECT_INT_EQUAL(COLUMN_NAME, command.args.show_all.sort_by.column);
        EXPECT(!command.args.show_all.sort_by.ascending);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_insert(void) {
    START_TEST("Parser INSERT");
    Command command;

    {
        char line[] = "INSERT";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(INSERT ID=123 Name="John Doe" Mark=95)";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(INSERT ID=123 Name="John Doe" Mark=95 Name="John Doe")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(INSERT ID=123 Name="John Doe" Mark=95 Programme= "abc \\ xyz")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_INSERT, command.tag);
        EXPECT_INT_EQUAL(123, command.args.insert.id);
        EXPECT_STRING_EQUAL("John Doe", command.args.insert.row.name);
        EXPECT_INT_EQUAL(95, command.args.insert.row.mark);
        EXPECT_STRING_EQUAL("abc \\ xyz", command.args.insert.row.programme);
        Command_destroy(&command);
    }

    {
        char line[] = "INSERT ID=abc Name=\"John Doe\" Mark=95 Programme=\"CS\" SORT BY Name";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    END_TEST();
}

TEST parser_query(void) {
    START_TEST("Parser QUERY");
    Command command;

    {
        char line[] = "QUERY";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "QUERY ID";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "QUERY ID=\"1\"";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "QUERY ID > 1.2";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "QUERY Name > \"Jane\"";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "QUERY ID=123";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_QUERY, command.tag);

        Condition* filter = command.args.query.filter;
        EXPECT_INT_EQUAL(OP_EQ, filter->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, filter->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(123, filter->args.eq_gt_lt.value.ui);

        SortBy sort_by = command.args.query.sort_by;
        EXPECT_INT_EQUAL(COLUMN_ID, sort_by.column);
        EXPECT(sort_by.ascending);
        Command_destroy(&command);
    }

    {
        char line[] =
            "QUERY ID=1 OR NOT Mark > 1 AND Programme= \"abc\" OR \"xyz\" IN Name SORT BY Name ASC";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_QUERY, command.tag);

        // ... OR ...
        Condition* filter = command.args.query.filter;
        EXPECT_INT_EQUAL(OP_OR, filter->tag);
        // (ID=1) OR ...
        Condition* lhs = filter->args.and_or.lhs;
        EXPECT_INT_EQUAL(OP_OR, lhs->tag);
        // ID=1
        Condition* llhs = lhs->args.and_or.lhs;
        EXPECT_INT_EQUAL(OP_EQ, llhs->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, llhs->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(1, llhs->args.eq_gt_lt.value.ui);
        // (NOT Mark>1) AND (Programme="abc")
        Condition* rlhs = lhs->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_AND, rlhs->tag);
        // NOT Mark>1
        Condition* lrlhs = rlhs->args.and_or.lhs;
        Condition* rlrlhs = lrlhs->args.not.cond;
        EXPECT_INT_EQUAL(OP_GT, rlrlhs->tag);
        EXPECT_INT_EQUAL(COLUMN_MARK, rlrlhs->args.eq_gt_lt.column);
        EXPECT_FLOAT_EQUAL((float)1, rlrlhs->args.eq_gt_lt.value.f);
        // Programme="abc"
        Condition* rrlhs = rlhs->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_EQ, rrlhs->tag);
        EXPECT_INT_EQUAL(COLUMN_PROGRAMME, rrlhs->args.eq_gt_lt.column);
        EXPECT_STRING_EQUAL("abc", rrlhs->args.eq_gt_lt.value.s);
        // "xyz" IN Name
        Condition* rhs = filter->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_IN, rhs->tag);
        EXPECT_INT_EQUAL(COLUMN_NAME, rhs->args.in.column);
        EXPECT_STRING_EQUAL("xyz", rhs->args.in.value);

        SortBy sort_by = command.args.query.sort_by;
        EXPECT_INT_EQUAL(COLUMN_NAME, sort_by.column);
        EXPECT(sort_by.ascending);
        Command_destroy(&command);
    }

    {
        char line[] = "QUERY (((((ID>1)OR ID<000))))";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_QUERY, command.tag);

        Condition* filter = command.args.query.filter;
        EXPECT_INT_EQUAL(OP_OR, filter->tag);
        Condition* lhs = filter->args.and_or.lhs;
        EXPECT_INT_EQUAL(OP_GT, lhs->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, lhs->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(1, lhs->args.eq_gt_lt.value.ui);
        Condition* rhs = filter->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_LT, rhs->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, rhs->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(0, rhs->args.eq_gt_lt.value.ui);

        SortBy sort_by = command.args.query.sort_by;
        EXPECT_INT_EQUAL(COLUMN_ID, sort_by.column);
        EXPECT(sort_by.ascending);
        Command_destroy(&command);
    }

    {
        char line[] = "QUERY SORT BY Name";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    END_TEST();
}

TEST parser_show_summary(void) {
    START_TEST("Parser SHOW SUMMARY");
    Command command;

    {
        char line[] = "SHOW SUMMARY";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_SUMMARY, command.tag);
        EXPECT(NULL == command.args.show_summary.filter);
        Command_destroy(&command);
    }

    {
        char line[] = "SHOW SUMMARY Mark=\"1\"";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "SHOW SUMMARY Mark>50.0 SORT BY ID";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "SHOW SUMMARY Programme = \"math\"";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_SUMMARY, command.tag);
        Condition* filter = command.args.show_summary.filter;
        EXPECT_INT_EQUAL(OP_EQ, filter->tag);
        EXPECT_INT_EQUAL(COLUMN_PROGRAMME, filter->args.eq_gt_lt.column);
        EXPECT_STRING_EQUAL("math", filter->args.eq_gt_lt.value.s);
        Command_destroy(&command);
    }

    {
        char line[] = "SHOW SUMMARY NOT (ID>1000 OR (Programme=\"science\" OR ID = 42))";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SHOW_SUMMARY, command.tag);
        Condition* filter = command.args.show_summary.filter;
        EXPECT_INT_EQUAL(OP_NOT, filter->tag);
        // ... OR (...)
        Condition* rhs = filter->args.not.cond;
        EXPECT_INT_EQUAL(OP_OR, rhs->tag);
        // ID>1000
        Condition* lrhs = rhs->args.and_or.lhs;
        EXPECT_INT_EQUAL(OP_GT, lrhs->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, lrhs->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(1000, lrhs->args.eq_gt_lt.value.ui);
        // (Programme="science" OR ID=42)
        Condition* rrhs = rhs->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_OR, rrhs->tag);
        // Programme="science"
        Condition* lrrhs = rrhs->args.and_or.lhs;
        EXPECT_INT_EQUAL(OP_EQ, lrrhs->tag);
        EXPECT_INT_EQUAL(COLUMN_PROGRAMME, lrrhs->args.eq_gt_lt.column);
        EXPECT_STRING_EQUAL("science", lrrhs->args.eq_gt_lt.value.s);
        // ID=42
        Condition* rrrhs = rrhs->args.and_or.rhs;
        EXPECT_INT_EQUAL(OP_EQ, rrrhs->tag);
        EXPECT_INT_EQUAL(COLUMN_ID, rrrhs->args.eq_gt_lt.column);
        EXPECT_INT_EQUAL(42, rrrhs->args.eq_gt_lt.value.ui);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_update(void) {
    START_TEST("Parser UPDATE");
    Command command;

    {
        char line[] = "UPDATE";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(UPDATE ID=123 Name="John Doe" Mark=95)";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_UPDATE, command.tag);
        EXPECT_INT_EQUAL(123, command.args.update.id);
        EXPECT_STRING_EQUAL("John Doe", command.args.update.row.name);
        EXPECT_INT_EQUAL(95, command.args.update.row.mark);
        EXPECT_INT_EQUAL((1 << COLUMN_NAME) | (1 << COLUMN_MARK),
                         command.args.update.update_columns);
        Command_destroy(&command);
    }

    {
        char line[] = R"(UPDATE ID=123 Name="John Doe" Mark=95 Name="John Doe")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = R"(UPDATE ID=123 Name="John Doe" Mark=95 Programme= "abc \\ xyz")";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_UPDATE, command.tag);
        EXPECT_INT_EQUAL(123, command.args.update.id);
        EXPECT_STRING_EQUAL("John Doe", command.args.update.row.name);
        EXPECT_INT_EQUAL(95, command.args.update.row.mark);
        EXPECT_STRING_EQUAL("abc \\ xyz", command.args.update.row.programme);
        EXPECT_INT_EQUAL((1 << COLUMN_NAME) | (1 << COLUMN_PROGRAMME) | (1 << COLUMN_MARK),
                         command.args.update.update_columns);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_delete(void) {
    START_TEST("Parser DELETE");
    Command command;

    {
        char line[] = "DELETE";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "DELETE ID";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "DELETE ID=\"3\"";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "DELETE Mark=10";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    {
        char line[] = "DELETE ID=3";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_DELETE, command.tag);
        EXPECT_INT_EQUAL(3, command.args.delete.id);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_save(void) {
    START_TEST("Parser SAVE");
    Command command;

    {
        char line[] = "SAVE ";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SAVE, command.tag);
        EXPECT(NULL == command.args.save.filename);
        Command_destroy(&command);
    }

    {
        char line[] = R"(SAVE    "surrounded by 3 \\\"spaces.txt"   )";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(parse_command(&tokenizer, &command));
        EXPECT_INT_EQUAL(CMD_SAVE, command.tag);
        EXPECT_STRING_EQUAL(R"(   "surrounded by 3 \\\"spaces.txt"   )",
                            command.args.save.filename);
        Command_destroy(&command);
    }

    END_TEST();
}

TEST parser_unknown(void) {
    START_TEST("Parser unknown command");
    Command command;

    {
        char line[] = "SHOW";
        Tokenizer tokenizer = Tokenizer_create(line);
        EXPECT(!parse_command(&tokenizer, &command));
    }

    END_TEST();
}
