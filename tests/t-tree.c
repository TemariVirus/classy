#pragma once

#include <stddef.h>
#include <stdlib.h>

#include "../src/chunked-allocator.h"
#include "../src/t-tree.h"
#include "testing.h"

#define EXPECT_NODE_INVARIANTS(it)                                                                 \
    do {                                                                                           \
        TTreeIter iter = (it);                                                                     \
        if (iter.node_trace_len == 0) {                                                            \
            break;                                                                                 \
        }                                                                                          \
        TTreeNode* node = iter.node_trace[iter.node_trace_len - 1];                                \
        EXPECT(node->length > 0);                                                                  \
        EXPECT(node->last_id == node->ids[node->length - 1]);                                      \
        /* All IDs in a node's left subtree are less than the node's smallest ID. */               \
        if (node->left != NULL) {                                                                  \
            EXPECT(node->ids[0] > node->left->last_id);                                            \
        }                                                                                          \
        /* All IDs in a node's right subtree are greater than the node's largest ID. */            \
        if (node->right != NULL) {                                                                 \
            EXPECT(node->last_id < node->right->ids[0]);                                           \
        }                                                                                          \
        /* Internal nodes contain at least TTREE_NODE_MIN_LEN items. */                            \
        if (__ttree_node_kind(node) == TTREE_NODEKIND_INTERNAL) {                                  \
            EXPECT(node->length >= TTREE_NODE_MIN_LEN);                                            \
        }                                                                                          \
        /* The height difference between a node's 2 children is at most 1. */                      \
        EXPECT(__ttree_node_balance_factor(node) >= -1 && __ttree_node_balance_factor(node) <= 1); \
    } while (0)

int id_compare(const void* a, const void* b) {
    ID id_a = *(const ID*)a;
    ID id_b = *(const ID*)b;
    if (id_a < id_b) {
        return -1;
    } else if (id_a > id_b) {
        return 1;
    } else {
        return 0;
    }
}

TTreeNode* create_fake_node(TTreeNodeAllocator* allocator, uint8_t height) {
    TTreeNode* node = __ttree_node_create(allocator);
    node->length = 1;
    node->height = height;
    return node;
}

void mix_id(ID* id) {
    *id ^= (*id << 13);
    *id ^= (*id >> 17);
    *id ^= (*id << 5);
}

TEST ttree_get(void) {
    START_TEST("T-tree get");

    const int ROW_COUNT = 1000;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        bool inserted = TTree_insert(&tree, i, &(Row){.name = "test", .programme = "", .mark = i});
        EXPECT(inserted);
    }

    {
        Row* row = TTree_get(&tree, 0);
        EXPECT(row != NULL);
        EXPECT_STRING_EQUAL("test", row->name);
        EXPECT_STRING_EQUAL("", row->programme);
        EXPECT_FLOAT_EQUAL(0, row->mark);
    }
    {
        Row* row = TTree_get(&tree, 500);
        EXPECT(row != NULL);
        EXPECT_STRING_EQUAL("test", row->name);
        EXPECT_STRING_EQUAL("", row->programme);
        EXPECT_FLOAT_EQUAL(500, row->mark);
    }
    {
        Row* row = TTree_get(&tree, 999);
        EXPECT(row != NULL);
        EXPECT_STRING_EQUAL("test", row->name);
        EXPECT_STRING_EQUAL("", row->programme);
        EXPECT_FLOAT_EQUAL(999, row->mark);
    }
    {
        Row* row = TTree_get(&tree, 1000);
        EXPECT(row == NULL);
    }
    {
        Row* row = TTree_get(&tree, 12345);
        EXPECT(row == NULL);
    }

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_get_empty(void) {
    START_TEST("T-tree get empty");

    TTree tree = TTree_create();
    Row* row = TTree_get(&tree, 0);
    EXPECT(row == NULL);

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_insert(void) {
    START_TEST("T-tree insert");

    const int ROW_COUNT = 10000;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        bool inserted = TTree_insert(&tree, i, &(Row){.name = "test", .programme = "", .mark = i});
        EXPECT(inserted);
    }

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    for (ID i = 0; i < ROW_COUNT; i++) {
        EXPECT(TTree_iter_next(&it, &id, &row));
        EXPECT_INT_EQUAL(i, id);
        EXPECT_STRING_EQUAL("test", row->name);
        EXPECT_STRING_EQUAL("", row->programme);
        EXPECT_FLOAT_EQUAL(i, row->mark);
        EXPECT_NODE_INVARIANTS(it);
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_insert_duplicate(void) {
    START_TEST("T-tree insert");

    const int ROW_COUNT = 10000;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        bool inserted = TTree_insert(&tree, i, &(Row){.name = "test", .programme = "", .mark = i});
        EXPECT(inserted);
    }
    for (ID i = 0; i < ROW_COUNT; i++) {
        bool inserted = TTree_insert(&tree, i, &(Row){.name = "", .programme = "hi", .mark = i});
        EXPECT(!inserted);
    }

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    for (ID i = 0; i < ROW_COUNT; i++) {
        EXPECT(TTree_iter_next(&it, &id, &row));
        EXPECT_INT_EQUAL(i, id);
        EXPECT_STRING_EQUAL("test", row->name);
        EXPECT_STRING_EQUAL("", row->programme);
        EXPECT_FLOAT_EQUAL(i, row->mark);
        EXPECT_NODE_INVARIANTS(it);
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_remove(void) {
    START_TEST("T-tree remove");

    const int ROW_COUNT = 10000;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        bool inserted = TTree_insert(&tree, i, &(Row){.name = "", .programme = "6969", .mark = i});
        EXPECT(inserted);
    }
    // Remove every even ID
    for (ID i = 0; i < ROW_COUNT; i += 2) {
        EXPECT(TTree_remove(&tree, i));
    }

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    // Check all odd IDs
    for (ID i = 1; i < ROW_COUNT; i += 2) {
        EXPECT(TTree_iter_next(&it, &id, &row));
        EXPECT_INT_EQUAL(i, id);
        EXPECT_STRING_EQUAL("", row->name);
        EXPECT_STRING_EQUAL("6969", row->programme);
        EXPECT_FLOAT_EQUAL(i, row->mark);
        EXPECT_NODE_INVARIANTS(it);
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_remove_random(void) {
    START_TEST("T-tree remove random");

    const int ROW_COUNT = 1000000;
    // Seed chosen to not produce any collisions
    // Confirmed by printing out the ids and piping it through `sort | uniq -d`
    const ID id_seed = 0x69421;
    TTree tree = TTree_create();

    ID id = id_seed;
    for (int i = 0; i < ROW_COUNT; i++) {
        mix_id(&id);
        bool inserted =
            TTree_insert(&tree, id, &(Row){.name = "", .programme = "6969", .mark = id});
        EXPECT(inserted); // There should be no collisions for this seed
    }
    // Remove every other ID
    id = id_seed;
    for (int i = 0; i < ROW_COUNT / 2; i++) {
        mix_id(&id);
        mix_id(&id);
        // Try removing twice to test idempotency
        EXPECT(TTree_remove(&tree, id));
        EXPECT(!TTree_remove(&tree, id));
    }

    ID expected_ids[ROW_COUNT / 2];
    id = id_seed;
    for (int i = 1; i < ROW_COUNT; i += 2) {
        mix_id(&id);
        expected_ids[i / 2] = id;
        mix_id(&id);
    }
    qsort(expected_ids, ROW_COUNT / 2, sizeof(ID), id_compare);

    TTreeIter it = TTree_iter_start(&tree);
    Row* row;
    // Check every other ID
    for (int i = 0; i < ROW_COUNT / 2; i++) {
        ID expected_id = expected_ids[i];
        EXPECT(TTree_iter_next(&it, &id, &row));
        EXPECT_INT_EQUAL(expected_id, id);
        EXPECT_STRING_EQUAL("", row->name);
        EXPECT_STRING_EQUAL("6969", row->programme);
        EXPECT_FLOAT_EQUAL(expected_id, row->mark);
        EXPECT_NODE_INVARIANTS(it);
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_rebalance(void) {
    START_TEST("T-tree rebalance");

    TTree tree = TTree_create();
    tree.node_allocator = TTreeNodeAllocator_create();

    tree.root = create_fake_node(tree.node_allocator, 4);
    tree.root->left = create_fake_node(tree.node_allocator, 3);
    tree.root->left->right = create_fake_node(tree.node_allocator, 2);
    tree.root->left->right->left = create_fake_node(tree.node_allocator, 1);
    tree.root->right = create_fake_node(tree.node_allocator, 3);
    tree.root->right->right = create_fake_node(tree.node_allocator, 2);
    tree.root->right->right->left = create_fake_node(tree.node_allocator, 1);
    tree.root->right->right->right = create_fake_node(tree.node_allocator, 1);

    // Rebalance left subtree
    tree.root->left->right = __rebalance_subtree(tree.root->left->right);
    tree.root->left = __rebalance_subtree(tree.root->left);
    tree.root = __rebalance_subtree(tree.root);

    // Rebalance right subtree
    tree.root->right->right = __rebalance_subtree(tree.root->right->right);
    tree.root->right = __rebalance_subtree(tree.root->right);
    tree.root = __rebalance_subtree(tree.root);

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    while (TTree_iter_next(&it, &id, &row)) {
        TTreeNode* node = it.node_trace_len == 0 ? tree.root : it.node_trace[it.node_trace_len - 1];
        int8_t balance = __ttree_node_balance_factor(node);
        EXPECT(balance >= -1);
        EXPECT(balance <= 1);
    }

    // We can't call TTree_destroy here because it will fail to free the fake rows.
    END_TEST();
}

TEST ttree_remove_empty(void) {
    START_TEST("T-tree remove empty");

    TTree tree = TTree_create();
    EXPECT(!TTree_remove(&tree, 0));
    EXPECT(!TTree_remove(&tree, 42));
    EXPECT(tree.root == NULL);

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_iter_empty(void) {
    START_TEST("T-tree iterate empty");

    TTree tree = TTree_create();

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_bulk_insert(void) {
    START_TEST("T-tree bulk insert");

    const int ROW_COUNT = 50000;
    TTreeBulkInsert bulk = TTree_bulk_insert_start();

    for (ID i = 0; i < ROW_COUNT; i++) {
        TTree_bulk_insert(&bulk, i, &(Row){.name = "", .programme = "test", .mark = i});
    }
    TTree tree = TTree_bulk_insert_end(&bulk);

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    for (ID i = 0; i < ROW_COUNT; i++) {
        EXPECT(TTree_iter_next(&it, &id, &row));
        EXPECT_INT_EQUAL(i, id);
        EXPECT_STRING_EQUAL("", row->name);
        EXPECT_STRING_EQUAL("test", row->programme);
        EXPECT_FLOAT_EQUAL(i, row->mark);
        EXPECT_NODE_INVARIANTS(it);
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

TEST ttree_bulk_insert_empty(void) {
    START_TEST("T-tree bulk insert empty");

    TTreeBulkInsert bulk = TTree_bulk_insert_start();
    TTree tree = TTree_bulk_insert_end(&bulk);

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}
