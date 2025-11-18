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
        fprintf(stdout, "len: %zu \n", strlen(line));
    }

    fprintf(stdout, "\n");
    print_prompt(SYSTEM_NAME);
    fprintf(stdout, "Exiting Classy. Goodbye!\n");
    return 0;
}
