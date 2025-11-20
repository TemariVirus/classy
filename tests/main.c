#include "testing.h"

#include "db-io.c"
#include "t-tree.c"
#include "tokenizer.c"

int main(void) {
    db_from_file_missing_table_name();
    db_from_file_bad_format();
    db_from_file_unordered_id();
    db_from_file_missing_line_term();
    db_from_file_successful();
    db_from_file_empty();

    ttree_get();
    ttree_get_empty();
    ttree_insert();
    ttree_insert_duplicate();
    ttree_remove();
    ttree_remove_random();
    ttree_rebalance();
    ttree_remove_empty();
    ttree_iter_empty();
    ttree_bulk_insert();
    ttree_bulk_insert_empty();

    tokenizer_empty();
    tokenizer_unkown();
    tokenizer_int_and_float();
    tokenizer_string();
    tokenizer_mixed();

    print_test_summary();
    return 0;
}
