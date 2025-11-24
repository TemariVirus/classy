#define __STDC_WANT_LIB_EXT2__ 1
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "atomic-file.c"
#include "db.c"
#include "error.c"
#include "parser.c"
#include "quicksort.c"
#include "row.c"
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
// The trailing newline character is trimmed.
// `size` must be at least 2.
//
// Returns buf on success, NULL if the line is too long.
char* get_line(char* buf, size_t size) {
    assert(size >= 2);
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
        print_prompt(SYSTEM_NAME);
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

// Pretty prints the columns to stdout. This is meant to be used in tandem with `print_record`.
void print_columns(void) {
    fprintf(stdout, "%-10s %-25s %-35s %-6s\n", Column_name(COLUMN_ID), Column_name(COLUMN_NAME),
            Column_name(COLUMN_PROGRAMME), Column_name(COLUMN_MARK));
}

// Pretty prints the record to stdout. This is meant to be used in tandem with `print_columns`.
// The `temp_id` member of the record must be set to its respective ID.
void print_record(const Row* record) {
    fprintf(stdout, "%-10u %-25s %-35s %-6.1f\n", record->temp_id, record->name, record->programme,
            record->mark);
}

// Runs the HELP command.
void run_help(void) {
    fprintf(stdout,
            "Commands are single-line and cannot exceed %d characters in length.\n"
            "All commands are case-sensitive.\n",
            LINE_BUF_SIZE - 2);
    fprintf(stdout, "Available commands:\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "HELP\n"
                    "\n"
                    "Show this help message.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "OPEN filename\n"
                    "\n"
                    "Loads the file as the new active database, discarding the old active\n"
                    "database (if any).\n"
                    "`filename` is either an absolute path or a path relative to the\n"
                    "current working directory.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "SHOW ALL [SORT BY column [ASC|DESC]]\n"
                    "\n"
                    "Displays all rows in the active database.\n"
                    "If the sort column is not specified, it defaults to ID.\n"
                    "If the sort order is not specified, it defaults to ASC.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "INSERT ID=id Name=\"name\" Programme=\"programme\" Mark=mark\n"
                    "\n"
                    "Inserts a new row into the active database.\n"
                    "\n"
                    "ID is a 32-bit unsigned integer.\n"
                    "Name and Programme are strings and must be enclosed in double quotes "
                    "(\").\n"
                    "Mark is a single-precision floating-point number.\n"
                    "\n"
                    "The columns may be in any order, but must appear exactly once.\n"
                    "\n"
                    "Row values cannot contain newlines (\\n).\n"
                    "Double quotes (\") and backslashes (\\) within string values must be\n"
                    "escaped by preceding them with a backslash.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "QUERY condition\n"
                    "\n"
                    "Displays all rows in the active database that satisfy `condition`.\n"
                    "\n"
                    "`condition` may be composed of the following operations,\n"
                    "ordered by highest to lowest precedence:\n"
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
                    "QUERY (Mark > 50.6 OR NOT Programme = \"Computer Science\") AND "
                    "\"Alice \\\"in\\\" Wonderland\" IN Name\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "SHOW SUMMARY [condition]\n"
                    "\n"
                    "Displays a summary of all rows in the active database that satisfy "
                    "`condition`.\n"
                    "`condition` is as defined in QUERY.\n"
                    "If condition is not given, all rows in the active database are used.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "UPDATE ID=id column=value...\n"
                    "\n"
                    "Updates the values of the columns in the row with the specified\n"
                    "ID, if it exists.\n"
                    "\n"
                    "The columns may be in any order, but cannot appear more than once.\n"
                    "The ID column is required. The other columns are optional.\n"
                    "\n"
                    "`value` follows the same constraints as in INSERT.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "DELETE ID=id\n"
                    "\n"
                    "Deletes the row with the specified ID from the active database.\n"
                    "\n"
                    "--------------------------------------------------------------------\n"
                    "\n"
                    "SAVE [filename]\n"
                    "\n"
                    "Saves the active database to a file.\n"
                    "\n"
                    "`filename` is either an absolute path or a path relative to the\n"
                    "current working directory.\n"
                    "If `filename` already exists, it will be overwritten.\n"
                    "If `filename` is not specified, it defaults to the filename used in\n"
                    "the last OPEN or SAVE command.\n"
                    "\n"
                    "--------------------------------------------------------------------"
                    "\n");
}

// Runs the OPEN command. `db` is only overwritten if the operation is successful.
// Returns whether the operation was successful.
bool run_open(DB* db, const CmdOpenArgs* args) {
    // Binary mode needed for cross-platform line endings
    FILE* file = fopen(args->filename, "rb");
    if (file == NULL) {
        fprintf(stdout, "ERROR: Could not open file \"%s\".\n", args->filename);
        return false;
    }

    DB temp_db;
    DB_from_file__Error err = DB_from_file(file, &temp_db);
    fclose(file);
    switch (err) {
    case ERROR_OK: {
        DB_destroy(db);
        *db = temp_db;
        fprintf(stdout, "The database file \"%s\" was successfully opened.\n", args->filename);
        return true;
    }
    case ERROR_MISSING_TABLE_NAME: {
        fprintf(stdout, "ERROR: Missing table name in file \"%s\".\n", args->filename);
        return false;
    }
    case ERROR_BAD_DB_FORMAT: {
        fprintf(stdout, "ERROR: Unrecognised file format in file \"%s\".\n", args->filename);
        return false;
    }
    case ERROR_BAD_DB_COLUMN: {
        fprintf(stdout, "ERROR: Unknown, missing or duplicate column in file \"%s\".\n",
                args->filename);
        return false;
    }
    case ERROR_UNORDERED_ID: {
        fprintf(stdout, "ERROR: IDs in file \"%s\" are not in strictly increasing order.\n",
                args->filename);
        return false;
    }
    }
}

// Runs the SHOW ALL command.
void run_show_all(const DB* db, const CmdShowAllArgs* args) {
    if (db->row_count == 0) {
        fprintf(stdout, "There are no records in the table \"%s\".\n", db->table_name);
        return;
    }

    ID id;
    Row* record;
    TTreeIter it = TTree_iter_start(&db->data);
    // Iterator already iterates in ascending order of ID, no need to sort in that case
    if (args->sort_by.column == COLUMN_ID && args->sort_by.ascending) {
        fprintf(stdout, "Here are all the records in the table \"%s\".\n", db->table_name);
        print_columns();
        while (TTree_iter_next(&it, &id, &record)) {
            record->temp_id = id; // Used for printing
            print_record(record);
        }
        return;
    }

    // Copy records to flat array so they can be sorted
    Row** records = malloc(sizeof(Row*) * db->row_count);
    if (records == NULL) {
        fprintf(stdout, "ERROR: Out of memory.\n");
        return;
    }
    for (size_t i = 0; TTree_iter_next(&it, &id, &record); i++) {
        records[i] = record;
        records[i]->temp_id = id; // Used for sorting and printing
    }

    // Sort records
    quicksort(records, db->row_count, sizeof(Row*), cmp_row, &args->sort_by);

    // Print records
    fprintf(stdout, "Here are all the records in the table \"%s\".\n", db->table_name);
    print_columns();
    for (size_t i = 0; i < db->row_count; i++) {
        print_record(records[i]);
    }

    free(records);
}

// Runs the SHOW SUMMARY command.
void run_show_summary(const DB* db, const CmdSummaryArgs* args) {
    size_t record_count = 0;
    // Use double to prevent infinity when adding many finite floats
    double mark_sum = 0.0;
    float highest_mark;
    RecordList highest_mark_records = RecordList_create();
    float lowest_mark;
    RecordList lowest_mark_records = RecordList_create();

    // Generate summary
    ID id;
    Row* row;
    TTreeIter it = TTree_iter_start(&db->data);
    while (TTree_iter_next(&it, &id, &row)) {
        if (!Condition_eval(args->filter, id, row)) {
            continue;
        }

        // Special case for first record
        if (record_count == 0) {
            highest_mark = row->mark;
            lowest_mark = row->mark;
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

    // There is no summary to print if no records mathced
    if (record_count == 0) {
        fprintf(stdout, "No records matched. Run the INSERT command to add new records.\n");
        goto cleanup;
    }

    // Print summary
    double avg_mark = mark_sum / (double)record_count;
    fprintf(stdout, "Here is a summary of the table \"%s\".\n", db->table_name);
    fprintf(stdout, "Number of students matched: %zu\n", record_count);
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

cleanup:
    RecordList_destroy(&highest_mark_records);
    RecordList_destroy(&lowest_mark_records);
}

// Runs the INSERT command.
void run_insert(DB* db, const CmdInsertArgs* args) {
    if (TTree_insert(&db->data, args->id, &args->row)) {
        fprintf(stdout, "A new record with ID=%u was successfully inserted.\n", args->id);
        db->row_count++;
    } else {
        fprintf(stdout,
                "A record with ID=%u already exists. The database is left unchanged.\n"
                "Run the UPDATE command to update an existing record instead.\n",
                args->id);
    }
}

// Runs the QUERY command.
void run_query(const DB* db, const CmdQueryArgs* args) {
    ID id;
    Row* record;
    TTreeIter it = TTree_iter_start(&db->data);

    // Copy records to flat array so they can be sorted
    RecordList records = RecordList_create();
    while (TTree_iter_next(&it, &id, &record)) {
        if (!Condition_eval(args->filter, id, record)) {
            continue;
        }
        record->temp_id = id; // Used for sorting and printing
        RecordList_append(&records, record);
    }

    if (records.length == 0) {
        // No records to print
        fprintf(stdout, "There are no matching records in the table \"%s\".\n", db->table_name);
    } else {
        // Iterator already iterates in ascending order of ID, no need to sort in that case
        if (args->sort_by.column != COLUMN_ID || !args->sort_by.ascending) {
            quicksort(records.items, records.length, sizeof(Row*), cmp_row, &args->sort_by);
        }

        // Print records
        fprintf(stdout, "Here are all the matching records in the table \"%s\".\n", db->table_name);
        print_columns();
        for (size_t i = 0; i < records.length; i++) {
            print_record(RecordList_get(&records, i));
        }
    }

    RecordList_destroy(&records);
}

// Run the UPDATE command.
void run_update(const DB* db, const CmdUpdateArgs* args) {
    Row* record = TTree_get(&db->data, args->id);
    if (record == NULL) {
        fprintf(
            stdout,
            "The record with ID=%u does not exist. Run the INSERT command to insert it instead.\n",
            args->id);
        return;
    }

    // Find which columns should be updated and update them
    for (Column column = 0; column < COLUMN_COUNT; column++) {
        if (!ColumnsMask_get(&args->update_columns, column)) {
            continue;
        }

        switch (column) {
        case COLUMN_ID:
            // ID cannot be updated
            break;
        case COLUMN_NAME:
            free(record->name);
            record->name = strdup(args->row.name);
            break;
        case COLUMN_PROGRAMME:
            free(record->programme);
            record->programme = strdup(args->row.programme);
            break;
        case COLUMN_MARK:
            record->mark = args->row.mark;
            break;
        }
    }
    fprintf(stdout, "The record with ID=%u was successfully updated.\n", args->id);
}

// Runs the DELETE command.
void run_delete(DB* db, const CmdDeleteArgs* args) {
    if (TTree_get(&db->data, args->id) == NULL) {
        fprintf(stdout, "The record with ID=%u does not exist.\n", args->id);
        return;
    }

    // There is a record to delete, but ask the user for confirmation
    fprintf(stdout,
            "Are you sure you want to delete the record with ID=%u? Type \"Y\" to confirm or "
            "type \"N\" to cancel.\n",
            args->id);
    print_prompt(USER_NAME);
    char confirmation_buf[3];
    char* confirmation = get_line(confirmation_buf, sizeof(confirmation_buf));

    print_prompt(SYSTEM_NAME);
    if (confirmation == NULL || strcmp(confirmation, "Y") != 0) {
        // User did not type Y
        fprintf(stdout, "The deletion was cancelled.\n");
        return;
    }
    // User typed Y
    TTree_remove(&db->data, args->id);
    fprintf(stdout, "The record with ID=%u was successfully deleted.\n", args->id);
    db->row_count--;
}

// Runs the SAVE command.
// Returns whether the operation was successful.
bool run_save(const DB* db, const CmdSaveArgs* args) {
    // Binary mode needed for cross-platform line endings
    AtomicFile* file = AtomicFile_open("wb");
    if (file == NULL) {
        fprintf(stdout, "ERROR: Could not open file for safe writing.\n");
        goto error_cleanup;
    }

    if (!DB_to_file(db, file->fptr)) {
        fprintf(stdout, "ERROR: Failed to serialize the table \"%s\"\n", db->table_name);
        goto error_cleanup;
    }
    if (!AtomicFile_close(file, args->filename)) {
        fprintf(stdout, "ERROR: Could not save to the file \"%s\".\n", args->filename);
        goto error_cleanup;
    }

    fprintf(stdout, "The database file \"%s\" was successfully saved.\n", args->filename);
    return true;

error_cleanup:
    AtomicFile_delete(file);
    return false;
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
        parse_command__Error parse_err = parse_command(&tokenizer, &cmd);
        switch (parse_err) {
        case ERROR_OK:
            break;
        case ERROR_EXPECTED_EOF:
            fprintf(stdout, "ERROR: Unexpected input after command.\n");
            break;
        case ERROR_EXPECTED_COMMAND:
            fprintf(stdout, "ERROR: Expected command keyword.\n");
            break;
        case ERROR_EXPECTED_VALUE:
            fprintf(stdout, "ERROR: Expected value.\n");
            break;
        case ERROR_EXPECTED_INT:
            fprintf(stdout, "ERROR: Expected integer.\n");
            break;
        case ERROR_EXPECTED_FLOAT:
            fprintf(stdout, "ERROR: Expected float.\n");
            break;
        case ERROR_EXPECTED_STR:
            fprintf(stdout, "ERROR: Expected string.\n");
            break;
        case ERROR_EXPECTED_COL:
            fprintf(stdout, "ERROR: Expected column name.\n");
            break;
        case ERROR_EXPECTED_ID_COL:
            fprintf(stdout, "ERROR: Expected ID column.\n");
            break;
        case ERROR_EXPECTED_STR_COL:
            fprintf(stdout, "ERROR: Expected a string column.\n");
            break;
        case ERROR_EXPECTED_OP_EQ:
            fprintf(stdout, "ERROR: Expected '='.\n");
            break;
        case ERROR_EXPECTED_COND:
            fprintf(stdout, "ERROR: Expected condition.\n");
            break;
        case ERROR_DUPLICATE_COL:
            fprintf(stdout, "ERROR: Duplicate column.\n");
            break;
        case ERROR_MISMATCHED_PAREN:
            fprintf(stdout, "ERROR: Mismatched parenthesis.\n");
            break;
        case ERROR_BAD_OP:
            fprintf(stdout, "ERROR: Illegal operator.\n");
            break;
        case ERROR_OUT_OF_MEM:
            fprintf(stdout, "ERROR: Out of memory.\n");
            continue;
        }
        if (parse_err != parse_command__ok) {
            fprintf(stdout, "For help on command syntax, run the HELP command.\n");
            continue;
        }

        switch (cmd.tag) {
        case CMD_HELP:
            run_help();
            break;
        case CMD_OPEN:
            if (run_open(&db, &cmd.args.open)) {
                free(last_filename);
                last_filename = strdup(cmd.args.open.filename);
            }
            break;
        case CMD_SHOW_ALL:
            if (!warn_no_db(&db)) {
                run_show_all(&db, &cmd.args.show_all);
            }
            break;
        case CMD_SHOW_SUMMARY:
            if (!warn_no_db(&db)) {
                run_show_summary(&db, &cmd.args.show_summary);
            }
            break;
        case CMD_INSERT:
            if (!warn_no_db(&db)) {
                run_insert(&db, &cmd.args.insert);
            }
            break;
        case CMD_QUERY:
            if (!warn_no_db(&db)) {
                run_query(&db, &cmd.args.query);
            }
            break;
        case CMD_UPDATE:
            if (!warn_no_db(&db)) {
                run_update(&db, &cmd.args.update);
            }
            break;
        case CMD_DELETE:
            if (!warn_no_db(&db)) {
                run_delete(&db, &cmd.args.delete);
            }
            break;
        case CMD_SAVE:
            if (cmd.args.save.filename == NULL) {
                cmd.args.save.filename = last_filename;
            }
            if (!warn_no_db(&db)) {
                if (!run_save(&db, &cmd.args.save)) {
                    break;
                }
                free(last_filename);
                last_filename = strdup(cmd.args.save.filename);
            }
            break;
        }

        Command_destroy(&cmd);
    }

    fprintf(stdout, "\n");
    print_prompt(SYSTEM_NAME);
    fprintf(stdout, "Exiting Classy. Goodbye!\n");
    return 0;
}
