#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define LINE_BUF_SIZE 4096
#define USER_NAME "P1_1"
#define SYSTEM_NAME "CMS"

void print_startup_message(void) {
    // Message to guide new user
    fprintf(stdout, "Welcome to Classy, a class management system!\n");
    fprintf(stdout, "Type a single line command here, and press <Enter> to run it.\n");
    fprintf(stdout, "Exit Classy by pressing ctrl+c.\n");
    fprintf(stdout, "For help on command syntax, run the \"HELP\" command.\n");
}

void print_help(void) {
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

void print_prompt(const char* prompt_name) { fprintf(stdout, "%s: ", prompt_name); }

// Reads a line from stdin into buf of size 'size'.
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
        for (int c = fgetc(stdin); c != '\n' && c != EOF;) {
            c = fgetc(stdin);
        }
    }
    return NULL;
}

int main(void) {
#if defined(_WIN32)
    // Needed on Windows to print utf-8 to the terminal
    SetConsoleOutputCP(65001);
#endif
    print_startup_message();

    char line_buf[LINE_BUF_SIZE];
    while (!feof(stdin)) {
        print_prompt(USER_NAME);
        char* line = get_line(line_buf, LINE_BUF_SIZE);
        if (line == NULL) {
            continue;
        }

        print_prompt(SYSTEM_NAME);
        print_help();
        fprintf(stdout, "input len: %zu \n", strlen(line));
    }

    fprintf(stdout, "\n");
    print_prompt(SYSTEM_NAME);
    fprintf(stdout, "Exiting Classy. Goodbye!\n");
    return 0;
}
