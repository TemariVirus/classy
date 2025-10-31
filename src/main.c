#include <stdint.h>
#include <stdio.h>

#include "row.c"
#include "t-tree.c"
#include <malloc.h>

int main(void) {
    TTree tree = TTree_create();

    uint32_t j = 0xDEADBEEF;
    for (int i = 0; i < 1000000; i++) {
        j ^= (j << 13);
        j ^= (j >> 17);
        j ^= (j << 5);
        TTree_put(&tree, j & 0xFFFFFF, &(Row){.name = "Alice", .programme = "Math", .mark = i});
    }

    for (int i = 0; i < 1000000; i++) {
        j ^= (j << 13);
        j ^= (j >> 17);
        j ^= (j << 5);
        TTree_remove(&tree, j & 0xFFFFFF);
    }

    ID id;
    Row* row;
    TTreeIter it = TTree_iter_start(&tree);
    while (TTree_iter_next(&it, &id, &row)) {
        // Process id and row
        printf("%10i: name=%s\t programme=%s\t mark=%f\n", id, row->name, row->programme,
               row->mark);
    }

    return 0;
}
