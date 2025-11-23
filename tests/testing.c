#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int tests_passed = 0;
unsigned int tests_skipped = 0;
unsigned int tests_failed = 0;

#define CSI "\x1B["
#define COLOR_RED 31
#define COLOR_GREEN 32
#define COLOR_YELLOW 93

void print_bold(const char* fmt, ...) {
    printf("%s1m", CSI);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("%s0m", CSI);
}

void print_colored(int color, const char* fmt, ...) {
    printf("%s%dm", CSI, color);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("%s0m", CSI);
}

void print_test_header(const char* file, int line, const char* test_name) {
    print_bold("%s:%d", file, line);
    printf(": in test \"%s\"\n", test_name);
}

// Make tests static functions so that the compiler will warn about unused tests.
#define TEST static void

#define START_TEST(name) const char* test_name = name;

#define END_TEST()                                                                                 \
    do {                                                                                           \
        (void)test_name;                                                                           \
        tests_passed++;                                                                            \
        return;                                                                                    \
    } while (0)

#define SKIP_TEST()                                                                                \
    do {                                                                                           \
        (void)test_name;                                                                           \
        tests_skipped++;                                                                           \
        return;                                                                                    \
    } while (0)

#define EXPECT(expected)                                                                           \
    do {                                                                                           \
        if (!(expected)) {                                                                         \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected true, found false\n");                                                \
            print_test_header(__FILE__, __LINE__, test_name);                                      \
            printf("    EXPECT(%s)\n", #expected);                                                 \
            print_colored(COLOR_GREEN, "    ^\n");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_INT_EQUAL(expected, actual)                                                         \
    do {                                                                                           \
        long long exp = (expected);                                                                \
        long long act = (actual);                                                                  \
        if (exp != act) {                                                                          \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected %lli, found %lli\n", exp, act);                                       \
            print_test_header(__FILE__, __LINE__, test_name);                                      \
            printf("    EXPECT_INT_EQUAL(%s, %s)\n", #expected, #actual);                          \
            print_colored(COLOR_GREEN, "    ^\n");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT_EQUAL(expected, actual)                                                       \
    do {                                                                                           \
        double exp = (expected);                                                                   \
        double act = (actual);                                                                     \
        if (exp != act) {                                                                          \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected %.17g, found %.17g\n", exp, act);                                     \
            print_test_header(__FILE__, __LINE__, test_name);                                      \
            printf("    EXPECT_FLOAT_EQUAL(%s, %s)\n", #expected, #actual);                        \
            print_colored(COLOR_GREEN, "    ^\n");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_STRING_EQUAL(expected, actual)                                                      \
    do {                                                                                           \
        char* exp = (expected);                                                                    \
        char* act = (actual);                                                                      \
        if (strcmp(exp, act) != 0) {                                                               \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected \"%s\"\n", exp);                                                      \
            printf("Found    \"%s\"\n", act);                                                      \
            print_test_header(__FILE__, __LINE__, test_name);                                      \
            printf("    EXPECT_STRING_EQUAL(%s, %s)\n", #expected, #actual);                       \
            print_colored(COLOR_GREEN, "    ^\n");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

void print_test_summary(void) {
    printf("\n");
    printf("Tests passed:  ");
    if (tests_failed == 0) {
        print_colored(COLOR_GREEN, "%u\n", tests_passed);
    } else {
        printf("%u\n", tests_passed);
    }

    printf("Tests skipped: ");
    if (tests_skipped == 0) {
        printf("%u\n", tests_skipped);
    } else {
        print_colored(COLOR_YELLOW, "%u\n", tests_skipped);
    }

    printf("Tests failed:  ");
    if (tests_failed == 0) {
        printf("%u\n", tests_failed);
    } else {
        print_colored(COLOR_RED, "%u\n", tests_failed);
    }

    if (tests_failed != 0) {
        exit(1);
    }
}
