#include "testing.h"

#include "t-tree.c"

int main(void) {
    ttree_insert();
    ttree_remove();
    ttree_remove_random();
    ttree_rebalance();
    ttree_remove_empty();
    ttree_iter_empty();

    print_test_summary();
    return 0;
}
