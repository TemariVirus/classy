#include <stdbool.h>
#include <stdio.h>

#include "testing.h"

void testme(void) {
    START_TEST("example");

    EXPECT(true);
    EXPECT_INT_EQUAL(1, 1);
    EXPECT_FLOAT_EQUAL(2, 2);
    EXPECT_STRING_EQUAL("32", "32");

    END_TEST();
}

int main(void) {
    testme();
    print_test_summary();
    return 0;
}
