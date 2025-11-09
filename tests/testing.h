#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int tests_passed = 0;
unsigned int tests_skipped = 0;
unsigned int tests_failed = 0;

#define START_TEST(name) const char* test_name = name;

#define END_TEST()                                                                                 \
    do {                                                                                           \
        tests_passed++;                                                                            \
        return;                                                                                    \
    } while (0)

#define SKIP_TEST()                                                                                \
    do {                                                                                           \
        tests_skipped++;                                                                           \
        return;                                                                                    \
    } while (0)

#define EXPECT(expected)                                                                           \
    do {                                                                                           \
        if (!(expected)) {                                                                         \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected true, found false\n");                                                \
            printf("%s:%d: in test \"%s\"\n", __FILE__, __LINE__, test_name);                      \
            printf("    EXPECT(%s)\n", #expected);                                                 \
            printf("    ^\n");                                                                     \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_INT_EQUAL(expected, actual)                                                         \
    do {                                                                                           \
        if ((expected) != (actual)) {                                                              \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected %lli, found %lli\n", (long long)(expected), (long long)(actual));     \
            printf("%s:%d: in test \"%s\"\n", __FILE__, __LINE__, test_name);                      \
            printf("    EXPECT_INT_EQUAL(%s, %s)\n", #expected, #actual);                          \
            printf("    ^\n");                                                                     \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT_EQUAL(expected, actual)                                                       \
    do {                                                                                           \
        if ((expected) != (actual)) {                                                              \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected %.17g, found %.17g\n", (double)(expected), (double)(actual));         \
            printf("%s:%d: in test \"%s\"\n", __FILE__, __LINE__, test_name);                      \
            printf("    EXPECT_FLOAT_EQUAL(%s, %s)\n", #expected, #actual);                        \
            printf("    ^\n");                                                                     \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_STRING_EQUAL(expected, actual)                                                      \
    do {                                                                                           \
        if (strcmp((expected), (actual)) != 0) {                                                   \
            tests_failed++;                                                                        \
            printf("\n");                                                                          \
            printf("Expected \"%s\"\n", (expected));                                               \
            printf("Found    \"%s\"\n", (actual));                                                 \
            printf("%s:%d: in test \"%s\"\n", __FILE__, __LINE__, test_name);                      \
            printf("    EXPECT_STRING_EQUAL(%s, %s)\n", #expected, #actual);                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

void print_test_summary(void) {
    printf("\n");
    printf("Tests passed:  %u\n", tests_passed);
    printf("Tests skipped: %u\n", tests_skipped);
    printf("Tests failed:  %u\n", tests_failed);
    if (tests_failed != 0) {
        exit(1);
    }
}
