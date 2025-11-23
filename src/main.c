#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "db.c"
#include "error.c"
#include "parser.c"
#include "tokenizer.c"

#define TYPE Row*
#define TYPED(THING) Record##THING
#include "list.c"

#define LINE_BUF_SIZE 4096
#define USER_NAME "P1_1"
#define SYSTEM_NAME "CMS"

void print_startup_message(void) {
    // Message to guide new user
    fprintf(stdout, "Welcome to Classy, a class management system!\n"
                    "Type a single line command here, and press enter to run it.\n"
                    "Exit Classy by pressing ctrl+c.\n"
                    "For help on command syntax, run the HELP command.\n");
}

void print_prompt(const char* prompt_name) { fprintf(stdout, "%s: ", prompt_name); }

// Reads a line from stdin into buf of size `size`.
// Returns buf on success, NULL if the line is too long.
char* get_line(char* buf, size_t size) {
    if (fgets(buf, size, stdin) == NULL) {
        // EOF or error
        return NULL;
    }

    // Ensure the whole line fits in the buffer
    size_t newline_offset = strcspn(buf, "\n");
    if (buf[newline_offset] == '\n') {
        // Remove newline character
        buf[newline_offset] = '\0';
        return buf;
    }

    // Line too long
    if (newline_offset >= size - 1) {
        fprintf(stdout, "Input line too long. Maximum length is %zu characters.\n", size - 2);
        // Clear the rest of the line from stdin
        for (int c = fgetc(stdin); c != '\n' && c != EOF; c = fgetc(stdin)) {
        }
    }
    return NULL;
}

// Prints a warning to stdout if `db` is not initialised (i.e., it has no table name).
// Returns whether the warning was printed.
bool warn_no_db(const DB* db) {
    if (db->table_name == NULL) {
        fprintf(stdout, "There is no active database. Run the OPEN command to load an active "
                        "database from a file.\n");
        return true;
    }
    return false;
}

// Runs the HELP command.
void run_help(void) {
    fprintf(stdout,
            "Commands are single-line and cannot exceed %d characters in length. All commands are "
            "case-sensitive.\n",
            LINE_BUF_SIZE - 2);
    fprintf(
        stdout,
        "Available commands:\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "HELP\n"
        "\n"
        "Show this help message.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "OPEN filename\n"
        "\n"
        "Loads the file as the new active database, discarding the old active database (if any).\n"
        "`filename` is either an absolute path or a path relative to the current working "
        "directory.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "SHOW ALL [SORT BY column [ASC|DESC]]\n"
        "\n"
        "Displays all rows in the active database.\n"
        "If the sort column is not specified, it defaults to ID.\n"
        "If the sort order is not specified, it defaults to ASC.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "INSERT ID=id Name=\"name\" Programme=\"programme\" Mark=mark\n"
        "\n"
        "Inserts a new row into the active database.\n"
        "\n"
        "ID is a 32-bit unsigned integer.\n"
        "Name and Programme are strings and must be enclosed in double quotes (\").\n"
        "Mark is a single-precision floating-point number.\n"
        "\n"
        "The columns may be in any order, but must appear exactly once.\n"
        "\n"
        "Row values cannot contain newlines (\\n).\n"
        "Double quotes (\") and backslashes (\\) within string values must be escaped by "
        "preceding them with a backslash.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "QUERY condition\n"
        "\n"
        "Displays all rows in the active database that satisfy `condition`.\n"
        "\n"
        "`condition` may be composed of the following operations (ordered by highest to lowest "
        "precedence):\n"
        "- integer, float, and string equality (column = val)\n"
        "- integer and float greater than (column > val)\n"
        "- integer and float less than (column < val)\n"
        "- string contains (val IN column)\n"
        "- boolean not (NOT expr)\n"
        "- boolean and (expr AND expr)\n"
        "- boolean or (expr OR expr)\n"
        "\n"
        "Parentheses may be used to group sub-conditions.\n"
        "If necessary, integers will be automatically converted to floats.\n"
        "Whitespace around the =, >, and < operators is ignored.\n"
        "\n"
        "Examples:\n"
        "QUERY ID=12345\n"
        "QUERY (Mark > 50.6 OR NOT Programme = \"Computer Science\") AND \"Alice \\\"in\\\" "
        "Wonderland\" IN Name\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "SHOW SUMMARY [condition]\n"
        "\n"
        "Displays a summary of all rows in the active database that satisfy `condition`.\n"
        "`condition` is as defined in QUERY.\n"
        "If condition is not given, all rows in the active database are used.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "UPDATE ID=id column=value...\n"
        "\n"
        "Updates the values of the other columns in the row with the specified ID, if it exists.\n"
        "\n"
        "The columns may be in any order, but cannot appear more than once.\n"
        "The ID column is required. The other columns are optional.\n"
        "\n"
        "`value` follows the same constraints as in INSERT.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "DELETE ID=id\n"
        "\n"
        "Deletes the row with the specified ID from the active database.\n"
        "\n"
        "----------------------------------------------------------------------------------------\n"
        "\n"
        "SAVE [filename]\n"
        "\n"
        "Saves the active database to a file.\n"
        "\n"
        "`filename` is either an absolute path or a path relative to the current working "
        "directory.\n"
        "If `filename` already exists, it will be overwritten.\n"
        "If `filename` is not specified, it defaults to the filename used in the last OPEN or SAVE "
        "command.\n"
        "\n"
        "----------------------------------------------------------------------------------------"
        "\n");
}

// Runs the OPEN command. `db` is only overwritten if the operation is successful.
// Returns whether the operation was successful.
bool run_open(const char* filename, DB* db) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stdout, "Error: Could not open file \"%s\".\n", filename);
        return false;
    }

    DB temp_db;
    DB_from_file__Error err = DB_from_file(file, &temp_db);
    fclose(file);
    switch (err) {
    case ERROR_OK: {
        DB_destroy(db);
        *db = temp_db;
        fprintf(stdout, "The database file \"%s\" was successfully opened.\n", filename);
        return true;
    }
    case ERROR_MISSING_TABLE_NAME: {
        fprintf(stdout, "Error: Missing table name in file \"%s\".\n", filename);
        return false;
    }
    case ERROR_BAD_DB_FORMAT: {
        fprintf(stdout, "Error: Unrecognised file format in file \"%s\".\n", filename);
        return false;
    }
    case ERROR_BAD_DB_COLUMN: {
        fprintf(stdout, "Error: Unknown, missing or duplicate column in file \"%s\".\n", filename);
        return false;
    }
    case ERROR_UNORDERED_ID: {
        fprintf(stdout, "Error: IDs in file \"%s\" are not in strictly increasing order.\n",
                filename);
        return false;
    }
    }
}

// Runs the SHOW SUMMARY command.
void run_show_summary(const DB* db, const Condition* filter) {
    if (db->row_count == 0) {
        fprintf(stdout, "There are no records in the table \"%s\".\n", db->table_name);
        return;
    }

    ID id;
    Row* row;
    TTreeIter it = TTree_iter_start(&db->data);

    // First record
    assert(TTree_iter_next(&it, &id, &row)); // We know db is not empty
    uint32_t record_count = 1;
    double mark_sum = row->mark; // Use double to prevent infinity when adding many finite floats
    float highest_mark = row->mark;
    RecordList highest_mark_records = RecordList_create();
    RecordList_append(&highest_mark_records, row);
    float lowest_mark = row->mark;
    RecordList lowest_mark_records = RecordList_create();
    RecordList_append(&lowest_mark_records, row);

    // Remaining records
    while (TTree_iter_next(&it, &id, &row)) {
        if (!Condition_eval(filter, id, row)) {
            continue;
        }

        record_count++;
        mark_sum += row->mark;
        // Check for new record with highest mark
        if (row->mark >= highest_mark) {
            if (row->mark > highest_mark) {
                highest_mark = row->mark;
                RecordList_clear(&highest_mark_records);
            }
            RecordList_append(&highest_mark_records, row);
        }
        // Check for new record with lowest mark
        if (row->mark <= lowest_mark) {
            if (row->mark < lowest_mark) {
                lowest_mark = row->mark;
                RecordList_clear(&lowest_mark_records);
            }
            RecordList_append(&lowest_mark_records, row);
        }
    }

    double avg_mark = mark_sum / (double)record_count;
    fprintf(stdout, "Here is a summary of the table \"%s\".\n", db->table_name);
    fprintf(stdout, "Number of students matched: %d\n", record_count);
    fprintf(stdout, "Average mark:               %.1f\n", avg_mark);

    fprintf(stdout, "Highest mark:               %.1f by ", highest_mark);
    assert(highest_mark_records.length > 0);
    fprintf(stdout, "%s", highest_mark_records.items[0]->name);
    for (size_t i = 1; i < highest_mark_records.length; i++) {
        Row* row = RecordList_get(&highest_mark_records, i);
        fprintf(stdout, ", %s", row->name);
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "Lowest mark:                %.1f by ", lowest_mark);
    assert(lowest_mark_records.length > 0);
    fprintf(stdout, "%s", lowest_mark_records.items[0]->name);
    for (size_t i = 1; i < lowest_mark_records.length; i++) {
        Row* row = RecordList_get(&lowest_mark_records, i);
        fprintf(stdout, ", %s", row->name);
    }
    fprintf(stdout, "\n");

    RecordList_destroy(&highest_mark_records);
    RecordList_destroy(&lowest_mark_records);
}

int main(void) {
#if defined(_WIN32)
    // Needed on Windows to print utf-8 to the terminal
    SetConsoleOutputCP(65001);
#endif
    print_startup_message();

    DB db = DB_create();
    char* last_filename = NULL;

    char line_buf[LINE_BUF_SIZE];
    while (!feof(stdin)) {
        print_prompt(USER_NAME);
        char* line = get_line(line_buf, LINE_BUF_SIZE);
        if (line == NULL) {
            continue;
        }
        print_prompt(SYSTEM_NAME);

        Command cmd;
        Tokenizer tokenizer = Tokenizer_create(line);
        if (!parse_command(&tokenizer, &cmd)) {
            fprintf(stdout, "Invalid command. For help on command syntax, run the HELP command.\n");
            continue;
        }

        switch (cmd.tag) {
        case CMD_HELP:
            run_help();
            break;
        case CMD_OPEN:
            if (run_open(cmd.args.open.filename, &db)) {
                free(last_filename);
                last_filename = strdup(cmd.args.open.filename);
            }
            break;
        case CMD_SHOW_SUMMARY:
            if (!warn_no_db(&db)) {
                run_show_summary(&db, cmd.args.show_summary.filter);
            }
            break;
        default:
            fprintf(stdout, "TODO\n");
            break;
        }

        Command_destroy(&cmd);
    }

    fprintf(stdout, "\n");
    print_prompt(SYSTEM_NAME);
    fprintf(stdout, "Exiting Classy. Goodbye!\n");
    return 0;
}
