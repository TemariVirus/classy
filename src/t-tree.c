#pragma once

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#include "row.c"

#define CACHE_SIZE 64 // Assume cache line is 64B
#define NODE_SIZE                                                                                  \
    ((CACHE_SIZE - sizeof(uint8_t) /* length */                                                    \
      - sizeof(uint8_t)            /* height */                                                    \
      - sizeof(void*)              /* left pointer */                                              \
      - sizeof(void*))             /* right pointer */                                             \
     / sizeof(ID))
#define NODE_MIN_LEN ((NODE_SIZE + 1) / 2)
// This is guaranteed to be enough for 4.94e14 students, or 144PiB of RAM.
// T-trees follow the same height bounds as AVL trees:
// https://en.wikipedia.org/wiki/AVL_tree#Properties
#define NODE_TRACE_SIZE 64

// Split ID from the rest of the data so that we can pack them more tightly in cache
typedef struct Node {
    ID ids[NODE_SIZE];
    // Number of IDs in this node.
    uint8_t length;
    uint8_t height;
    struct Node* left;
    struct Node* right;
    // TODO: make this an array of pointers to copy less and check performance diff
    // data must come last so that everything else is cache-aligned.
    Row data[NODE_SIZE];
} Node;

// The TTree owns the memory (and strings) of the nodes and data.
typedef struct {
    Node* root;
} TTree;

#define TYPE Node*
#define TYPED(THING) Node##THING
#include "list.c"

typedef struct {
    NodeList nodes;
    uint8_t pos;
} TTreeIter;

// Create an empty node.
Node* __create_node_empty(void) {
    // TODO: allocate in bigger blocks for better performance?
    // We align this to the cache line size so that all the important stuff
    // is guaranteed to fit in 1 cache line.
    Node* node = aligned_alloc(CACHE_SIZE, sizeof(Node));
    node->length = 0;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

// Create a new node with its first row.
Node* __create_node(ID id, const Row* row) {
    Node* node = __create_node_empty();
    node->ids[0] = id;
    node->length = 1;
    node->data[0] = Row_dupe(row);
    return node;
}

static inline uint8_t __node_height(Node* node) {
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

static inline void __update_node_height(Node* node) {
    if (node == NULL) {
        return;
    }
    uint8_t left_height = __node_height(node->left);
    uint8_t right_height = __node_height(node->right);
    node->height = 1 + (left_height > right_height ? left_height : right_height);
}

static inline int8_t __node_balance(Node* node) {
    if (node == NULL) {
        return 0;
    }
    return __node_height(node->left) - __node_height(node->right);
}

// Create an empty TTree.
TTree TTree_create(void) { return (TTree){.root = NULL}; }

// Perform a left rotation on the root node `node`. Returns the new root node.
Node* __leftRotate(Node* node) {
    assert(node != NULL);
    assert(node->right != NULL);
    Node* right = node->right;
    Node* right_left = right->left;
    right->left = node;
    node->right = right_left;
    __update_node_height(node);
    __update_node_height(right);
    return right;
}

// Perform a right rotation on the root node `node`. Returns the new root node.
Node* __rightRotate(Node* node) {
    assert(node != NULL);
    assert(node->left != NULL);
    Node* left = node->left;
    Node* left_right = left->right;
    left->right = node;
    node->left = left_right;
    __update_node_height(node);
    __update_node_height(left);
    return left;
}

// Rebalances the subtree with root node `node`. Returns the new root node.
Node* __rebalance_subtree(Node* node) {
    assert(node != NULL);
    if (__node_balance(node) > 1) {
        if (__node_balance(node->left) >= 0) {
            // Left left
            return __rightRotate(node);
        } else {
            // Left right
            node->left = __leftRotate(node->left);
            return __rightRotate(node);
        }
    }
    if (__node_balance(node) < -1) {
        if (__node_balance(node->right) <= 0) {
            // Right right
            return __leftRotate(node);
        } else {
            // Right left
            node->right = __rightRotate(node->right);
            return __leftRotate(node);
        }
    }
    // No balancing needed
    return node;
}

// Linear search for the position to insert `id` into the sorted array `ids` of length `end`.
size_t __linear_search(ID* ids, size_t ids_len, ID id) {
    size_t pos = 0;

    // Use SIMD for first 8 comparisions if possible (there are only 11)
#if defined(__AVX2__)
    // Add offset to convert to signed range for correct comparison
    const __m256i offset = _mm256_set1_epi32(INT32_MIN);
    __m256i id_vec = _mm256_set1_epi32(id + INT32_MIN);
    __m256i ids_vec = _mm256_loadu_si256((__m256i*)ids);
    ids_vec = _mm256_add_epi32(ids_vec, offset);
    __m256i cmp_mask = _mm256_cmpgt_epi32(id_vec, ids_vec);
    int mask = _mm256_movemask_ps((__m256)cmp_mask);
    pos += __builtin_ctz((1 << 8) | ~mask);
#endif

    while (pos < ids_len && id > ids[pos]) {
        pos++;
    }
    return pos > ids_len ? ids_len : pos;
}

// Sets out_pos to the position within the bounding node used for insertion of the given ID.
// Appends the traversed nodes to out_nodes, with the last node being the bounding node.
// out_nodes must have enough capacity to hold the traversed nodes.
// Returns whether the ID already exists.
bool __get_bounding(const TTree* tree, ID id, NodeList* out_nodes, size_t* out_pos) {
    assert(tree->root != NULL);

    // Search for bounding node, starting at root
    Node* node = tree->root;
    while (node != NULL) {
        NodeList_append_assume_capacity(out_nodes, node);
        size_t node_len = node->length;
        assert(node_len > 0);
        // id is too small, go left
        if (id < node->ids[0]) {
            node = node->left;
            continue;
        }
        // id is too big, go right
        if (id > node->ids[node_len - 1]) {
            node = node->right;
            continue;
        }

        // id is bounded by this node, linear scan for position
        size_t pos = __linear_search(node->ids, node_len, id);
        *out_pos = pos;
        return id == node->ids[pos];
    }

    // No bounding node, try to insert it into the last node
    assert(out_nodes->length > 0);
    Node* parent = NodeList_get(out_nodes, out_nodes->length - 1);
    bool insert_left = id < parent->ids[0];
    *out_pos = insert_left ? 0 : parent->length;
    return false;
}

void __rebalance_from_node_trace(TTree* tree, NodeList* node_trace) {
    assert(node_trace->length > 0);
    // Node was added/deleted, balance the tree again
    while (node_trace->length > 1) {
        Node* node = NodeList_pop(node_trace);
        assert(node != NULL);
        __update_node_height(node);
        Node* parent = NodeList_get(node_trace, node_trace->length - 1);
        Node** node_ptr = node == parent->left ? &parent->left : &parent->right;
        *node_ptr = __rebalance_subtree(node);
    }
    Node* root = NodeList_pop(node_trace);
    __update_node_height(root);
    tree->root = __rebalance_subtree(root);
}

// Get a row by ID. Returns NULL if not found.
Row* TTree_get(const TTree* tree, ID id) {
    // TODO: can we not allocate this for every get?
    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = (NodeList){
        .items = node_trace_buf,
        .length = 0,
        .capacity = sizeof(node_trace_buf) / sizeof(node_trace_buf[0]),
    };
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    if (exists) {
        Node* node = NodeList_get(&node_trace, node_trace.length - 1);
        return &node->data[pos];
    }
    return NULL;
}

// Insert or update a row by ID. Pointers in `row` are copied and do not need to be retained.
void TTree_put(TTree* tree, ID id, const Row* row) {
    if (tree->root == NULL) {
        tree->root = __create_node(id, row);
        return;
    }

    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = (NodeList){
        .items = node_trace_buf,
        .length = 0,
        .capacity = sizeof(node_trace_buf) / sizeof(node_trace_buf[0]),
    };
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    Node* node = NodeList_get(&node_trace, node_trace.length - 1);
    if (exists) {
        // Update existing row
        Row_destroy(&node->data[pos]);
        node->data[pos] = Row_dupe(row);
        return;
    }

    size_t node_len = node->length;
    assert(node_len > 0);
    if (node_len < NODE_SIZE) {
        // There's space, insert it here
        memmove(&node->ids[pos + 1], &node->ids[pos], (node_len - pos) * sizeof(ID));
        node->ids[pos] = id;
        node->length++;
        memmove(&node->data[pos + 1], &node->data[pos], (node_len - pos) * sizeof(Row));
        node->data[pos] = Row_dupe(row);
        return;
    }

    // No more space, create a new node if id is out of range
    if (pos == 0) {
        assert(node->left == NULL);
        node->left = __create_node(id, row);
        __rebalance_from_node_trace(tree, &node_trace);
        return;
    }
    if (pos == NODE_SIZE) {
        assert(node->right == NULL);
        node->right = __create_node(id, row);
        __rebalance_from_node_trace(tree, &node_trace);
        return;
    }

    // No more space, displace the smallest ID
    ID removed_id = node->ids[0];
    memmove(&node->ids[0], &node->ids[1], (pos - 1) * sizeof(ID));
    node->ids[pos - 1] = id;
    Row removed_row = node->data[0];
    memmove(&node->data[0], &node->data[1], (pos - 1) * sizeof(Row));
    node->data[pos - 1] = Row_dupe(row);

    // Insert the removed id into the left subtree
    if (node->left == NULL) {
        node->left = __create_node_empty();
    }
    Node* child = node->left;
    while (child->right != NULL) {
        NodeList_append_assume_capacity(&node_trace, child);
        child = child->right;
    }

    if (child->length < NODE_SIZE) {
        // We have space, insert it
        child->ids[child->length] = removed_id;
        child->data[child->length++] = removed_row;
        if (child->length > 1) {
            // No new node created, no need to rebalance
            return;
        }
    } else {
        // There is no space in the left subtree, insert it further down
        assert(child->right == NULL);
        child->right = __create_node(removed_id, &removed_row);
        // No need to balance a half-leaf node
        assert(child->height <= 2);
        __update_node_height(child);
    }

    __rebalance_from_node_trace(tree, &node_trace);
}

// Rebalance the subtree after a row was removed. note_ptr must point to a non-internal node.
// Return whether a node was deleted.
bool __rebalance_after_remove_non_internal(Node** node_ptr) {
    Node* node = *node_ptr;
    assert(node != NULL);
    assert(node->left == NULL || node->right == NULL);

    if (node->left == NULL && node->right == NULL) {
        // Leaf node, delete if empty
        if (node->length == 0) {
            free(node);
            *node_ptr = NULL;
            return true;
        }
        return false;
    }

    // Half-leaf node, try to merge with child
    Node* child = node->left != NULL ? node->left : node->right;
    // Balance factor cannot exceed +-1, so child must be a leaf node
    assert(child->left == NULL && child->right == NULL);
    if (node->length + child->length > NODE_SIZE) {
        return false;
    }

    // TODO: is there a better way to merge 2 sorted arrays?
    // Merge child into node
    size_t i_out = 0, i1 = 0, i2 = 0;
    Node node_copy = *node;
    while (i1 < node->length && i2 < child->length) {
        if (node->ids[i1] < child->ids[i2]) {
            node->ids[i_out] = node_copy.ids[i1];
            node->data[i_out++] = node_copy.data[i1++];
        } else {
            node->ids[i_out] = child->ids[i2];
            node->data[i_out++] = child->data[i2++];
        }
    }
    // Copy remaining items, only 0 or 1 of these pairs will run as memcpying 0 bytes does nothing.
    // (hooray for less branching!)
    assert((node->length - i1 == 0) || (child->length - i2 == 0));
    memcpy(&node->ids[i_out], &node_copy.ids[i1], (node->length - i1) * sizeof(ID));
    memcpy(&node->ids[i_out], &child->ids[i2], (child->length - i2) * sizeof(ID));
    memcpy(&node->data[i_out], &node_copy.data[i1], (node->length - i1) * sizeof(Row));
    memcpy(&node->data[i_out], &child->data[i2], (child->length - i2) * sizeof(Row));
    node->length += child->length;

    // We're deleting the only child, so we can avoid branching here
    assert(node->left == NULL || node->right == NULL);
    node->left = NULL;
    node->right = NULL;
    free(child);

    __update_node_height(node);
    *node_ptr = __rebalance_subtree(node);
    return true;
}

// Remove a row by ID. Returns whether a value was removed.
bool TTree_remove(TTree* tree, ID id) {
    if (tree->root == NULL) {
        return false;
    }

    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = (NodeList){
        .items = node_trace_buf,
        .length = 0,
        .capacity = sizeof(node_trace_buf) / sizeof(node_trace_buf[0]),
    };
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    Node* node = NodeList_get(&node_trace, node_trace.length - 1);
    if (!exists) {
        return false;
    }

    // Remove the id and row
    size_t node_len = node->length;
    memmove(&node->ids[pos], &node->ids[pos + 1], (node_len - pos - 1) * sizeof(ID));
    node->length--;
    Row_destroy(&node->data[pos]);
    memmove(&node->data[pos], &node->data[pos + 1], (node_len - pos - 1) * sizeof(Row));

    if (node->left == NULL || node->right == NULL) {
        // Half-leaf or leaf node
        if (node_trace.length <= 1) {
            __rebalance_after_remove_non_internal(&tree->root);
            return true;
        }

        node = NodeList_pop(&node_trace);
        Node* parent = NodeList_get(&node_trace, node_trace.length - 1);
        Node** node_ptr = node == parent->left ? &parent->left : &parent->right;
        bool deleted = __rebalance_after_remove_non_internal(node_ptr);
        node = parent;
        if (!deleted) {
            return true;
        }
        goto rebalance;
    }

    // Internal node, ensure min length
    if (node->length >= NODE_MIN_LEN) {
        return true;
    }

    // Steal a value from the right subtree
    Node* child = node->right;
    size_t subtree_start = node_trace.length;
    while (child->left != NULL) {
        NodeList_append_assume_capacity(&node_trace, child);
        child = child->left;
    }
    assert(child->length > 0);

    node->ids[node->length] = child->ids[0];
    memmove(&child->ids[0], &child->ids[1], (child->length - 1) * sizeof(ID));
    node->data[node->length] = child->data[0];
    memmove(&child->data[0], &child->data[1], (child->length - 1) * sizeof(Row));
    node->length++;
    child->length--;

    // Rebalance the child
    Node** child_ptr = node_trace.length == subtree_start
                           ? &node->right
                           : &NodeList_get(&node_trace, node_trace.length - 1)->left;
    bool deleted = __rebalance_after_remove_non_internal(child_ptr);
    if (!deleted) {
        return true;
    }

rebalance:
    __rebalance_from_node_trace(tree, &node_trace);
    return true;
}

// Remove and free all rows.
void TTree_remove_all(TTree* tree) {
    Node* root = tree->root;
    if (root == NULL) {
        return;
    }

    // Free subtrees
    TTree_remove_all(&(TTree){.root = root->left});
    TTree_remove_all(&(TTree){.root = root->right});
    // Free root
    for (size_t i = 0; i < root->length; i++) {
        Row_destroy(&root->data[i]);
    }
    free(root);
}

// Create an iterator starting at the beginning of the TTree.
TTreeIter TTree_iter_start(const TTree* tree) {
    NodeList nodes = NodeList_create();
    Node* node = tree->root;
    while (node != NULL) {
        NodeList_append(&nodes, node);
        node = node->left;
    }
    return (TTreeIter){
        .nodes = nodes,
        .pos = 0,
    };
}

// Advance the iterator to the next ID and Row.
// Returns whether there was a next element.
bool TTree_iter_next(TTreeIter* iter, ID* out_id, Row** out_row) {
    if (iter->nodes.length == 0) {
        return false;
    }

    Node* node = NodeList_get(&iter->nodes, iter->nodes.length - 1);
    *out_id = node->ids[iter->pos];
    *out_row = &node->data[iter->pos];
    if (++iter->pos < node->length) {
        return true;
    }

    iter->pos = 0;
    // Center finished, iterate right subtree
    if (node->right != NULL) {
        node = node->right;
        while (node != NULL) {
            NodeList_append(&iter->nodes, node);
            node = node->left;
        }
        return true;
    }

    // Entire subtree finished, go up to continue
    while (true) {
        Node* node = NodeList_pop(&iter->nodes);
        // We iterated all the nodes
        if (iter->nodes.length == 0) {
            break;
        }
        Node* parent = NodeList_get(&iter->nodes, iter->nodes.length - 1);
        // Left subtree finished, iterate center
        if (parent->left == node) {
            break;
        }
        // Right subtree finished, meaning this entire subtree is finished
        // and we need to go up again
    }
    return true;
}
