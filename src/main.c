#include <stdio.h>

#include "row.c"
#include "t-tree.c"
#include <malloc.h>

int main() {
    TTree tree = TTree_create();

    TTree_put(&tree, 123,
              &(Row){
                  .name = "Alices",
                  .programme = "Math",
                  .mark = 1.420,
              });
    for (int i = 0; i < 5000000; i++)
        TTree_put(&tree, i,
                  &(Row){
                      .name = "Alice",
                      .programme = "Maths",
                      .mark = 2.69,
                  });

    ID id;
    Row* row;
    TTreeIter it = TTree_iter_start(&tree);
    while (TTree_iter_next(&it, &id, &row)) {
        // Process id and row
        printf("%10i: name=%s\t programme=%s\t mark=%f\n", id, row->name, row->programme,
               row->mark);
        break;
    }

    return 0;
}
