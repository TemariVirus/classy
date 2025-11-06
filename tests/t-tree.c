#pragma once

#include "../src/t-tree.c"
#include "../src/chunked-allocator.c"
#include "testing.h"

Node* create_fake_node(NodeAllocator* allocator, uint8_t height) {
    Node* node = __create_node_empty(allocator);
    node->length = 1;
    node->height = height;
    return node;
}

void ttree_insert(void) {
    START_TEST("T-tree insert");

    const int ROW_COUNT = 100;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        TTree_put(&tree, i, &(Row){.name = "test", .programme = "", .mark = i});
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
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

void ttree_remove(void) {
    START_TEST("T-tree remove");

    const int ROW_COUNT = 100;
    TTree tree = TTree_create();

    for (ID i = 0; i < ROW_COUNT; i++) {
        TTree_put(&tree, i, &(Row){.name = "", .programme = "6969", .mark = i});
    }
    // Remove every even ID
    for (ID i = 0; i < ROW_COUNT; i += 2) {
        TTree_remove(&tree, i);
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
    }
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}

void ttree_rebalance(void) {
    START_TEST("T-tree rebalance");

    TTree tree = TTree_create();
    tree.node_allocator = NodeAllocator_create();

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
        Node* node =
            it.nodes.length == 0 ? tree.root : NodeList_get(&it.nodes, it.nodes.length - 1);
        int8_t balance = __node_balance(node);
        EXPECT(balance >= -1);
        EXPECT(balance <= 1);
    }

    // We can't call TTree_destroy here because it will fail to free the fake rows.
    END_TEST();
}

void ttree_remove_empty(void) {
    START_TEST("T-tree remove empty");

    TTree tree = TTree_create();
    TTree_remove(&tree, 0);
    TTree_remove(&tree, 42);
    EXPECT(tree.root == NULL);

    TTree_destroy(&tree);
    END_TEST();
}

void ttree_iter_empty(void) {
    START_TEST("T-tree iterate empty");

    TTree tree = TTree_create();

    TTreeIter it = TTree_iter_start(&tree);
    ID id;
    Row* row;
    EXPECT(!TTree_iter_next(&it, &id, &row));

    TTree_destroy(&tree);
    END_TEST();
}
