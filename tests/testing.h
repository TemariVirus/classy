#pragma once

#include <stdio.h>
#include <string.h>

unsigned int tests_run = 0;
unsigned int tests_passed = 0;

#define START_TEST(name)                                                                           \
    const char* test_name = name;                                                                  \
    tests_run++;
#define END_TEST() tests_passed++;

#define EXPECT(expected)                                                                           \
    do {                                                                                           \
        if (!(expected)) {                                                                         \
            printf("\nFailed test \"%s\": Expected %s\n", test_name, #expected);                   \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_INT_EQUAL(expected, actual)                                                         \
    do {                                                                                           \
        if ((expected) != (actual)) {                                                              \
            printf("\nFailed test \"%s\": Expected %lli, got %lli\n", test_name,                   \
                   (unsigned long long)(expected), (unsigned long long)(actual));                  \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT_EQUAL(expected, actual)                                                       \
    do {                                                                                           \
        if ((expected) != (actual)) {                                                              \
            printf("\nFailed test \"%s\": Expected %g, got %g\n", test_name, (double)(expected),   \
                   (double)(actual));                                                              \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define EXPECT_STRING_EQUAL(expected, actual)                                                      \
    do {                                                                                           \
        if (strcmp((expected), (actual)) != 0) {                                                   \
            printf("\nFailed test \"%s\":\n", test_name);                                          \
            printf("  Expected \"%s\"\n", (expected));                                             \
            printf("  Got      \"%s\"\n", (actual));                                               \
            return;                                                                                \
        }                                                                                          \
    } while (0)

void print_test_summary(void) {
    printf("\nTests run:    %u\n", tests_run);
    printf("Tests failed: %u\n", tests_run - tests_passed);
}
