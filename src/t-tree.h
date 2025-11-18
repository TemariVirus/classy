// Concrete T-tree implementation that stores rows and IDs in ascending order.
// A T-tree has serveral invariants (https://en.wikipedia.org/wiki/T-tree):
// - All IDs in a node are sorted in ascending order.
// - All IDs in a node's left subtree are less than the node's smallest ID.
// - All IDs in a node's right subtree are greater than the node's largest ID.
// - Internal nodes contain at least TTREE_NODE_MIN_LEN items.
// - The height difference between a node's 2 children is at most 1.
//
// Also see Wikipedia's article on AVL trees (https://en.wikipedia.org/wiki/AVL_tree)
// as the T-tree article lacks detail on tree balancing.

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "row.h"

#define CACHE_ALIGN 64 // Assume a cache line is 64B
// Maximum number of IDs in a node.
// Fastest node size found empirically.
#define TTREE_NODE_SIZE 38
// Minimum number of items in an internal node.
#define TTREE_NODE_MIN_LEN ((TTREE_NODE_SIZE + 1) / 2)
// Since there are only 2^32 unique IDs, the tree will never have more than 39 levels.
// T-trees follow the same height bounds as AVL trees:
// https://en.wikipedia.org/wiki/AVL_tree#Properties
//
// log_phi((2^32 / TTREE_NODE_MIN_LEN) + 2) - 0.3277 = 39.64...
#define TTREE_NODE_TRACE_LEN 39

// A node in the T-tree. Contains up to TTREE_NODE_SIZE IDs and Rows.
// ID is split from the rest of the data so that we can pack them more tightly in cache.
typedef struct TTreeNode {
    // Left child
    struct TTreeNode* left;
    // Right child
    struct TTreeNode* right;
    // Number of IDs in this node.
    uint8_t length;
    // Height of the subtree rooted at this node.
    // i.e., how many levels are below this node, including the node itself.
    uint8_t height;
    // Copy of the last ID to reduce cache misses during search.
    ID last_id;
    // IDs must always be sorted in ascending order.
    ID ids[TTREE_NODE_SIZE];
    // data[0] is associated with ids[0], etc.
    // data must come last so that everything else is cache-aligned.
    Row data[TTREE_NODE_SIZE];
} TTreeNode;
// Ensure that TTreeNode.left, TTreeNode.right, TTreeNode.ids[0] and TTreeNode.last_id all lie on
// the same cache line. These 4 fields are used in searching for the bounding node (which takes up
// the majority of time), so this speeds things up considerably.
static_assert(sizeof(TTreeNode) % CACHE_ALIGN == 0,
              "TTreeNode size must be a multiple of cache line size");

// The different kinds of nodes, depending on where they are in the tree.
typedef enum {
    // A leaf node has no children.
    TTREE_NODEKIND_LEAF,
    // A half-leaf node has one child.
    TTREE_NODEKIND_HALFLEAF,
    // An internal node has two children.
    TTREE_NODEKIND_INTERNAL,
} TTreeNodeKind;

// chunked-allocator for fast aligned allocation
#define TYPE TTreeNode
#define TYPED(THING) TTreeNode##THING
#include "chunked-allocator.h"

#define TYPE TTreeNode*
#define TYPED(THING) TTreeNode##THING
#include "list.h"

// The TTree owns the memory (and strings) of the all nodes.
typedef struct {
    // NULL only if the tree is empty.
    TTreeNode* root;
    // NULL only if the tree is empty.
    TTreeNodeAllocator* node_allocator;
} TTree;

// Return the minimum of `a` and `b`.
static int __min2(int a, int b) { return a < b ? a : b; }

// Return the maximum of `a` and `b`.
static int __max2(int a, int b) { return a > b ? a : b; }

// Create an empty node.
//
// The same allocator passed into this function must be used to free the node.
static TTreeNode* __ttree_node_create(TTreeNodeAllocator* allocator) {
    TTreeNode* node = TTreeNodeAllocator_alloc(allocator);
    node->left = NULL;
    node->right = NULL;
    node->length = 0;
    node->height = 1;
    return node;
}

// Get the kind of the given node.
static TTreeNodeKind __ttree_node_kind(const TTreeNode* node) {
    assert(node != NULL);
    if (node->left == NULL && node->right == NULL) {
        return TTREE_NODEKIND_LEAF;
    }
    if (node->left != NULL && node->right != NULL) {
        return TTREE_NODEKIND_INTERNAL;
    }
    return TTREE_NODEKIND_HALFLEAF;
}

// Get the height of the node's subtree.
//
// Also see `__update_node_height`.
static uint8_t __ttree_node_height(TTreeNode* node) {
    // NULL node is an empty subtree
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

// Update the height of the node's subtree. The height of the children must be correct.
//
// Also see `__ttree_node_height`.
static void __ttree_node_update_height(TTreeNode* node) {
    if (node == NULL) {
        return;
    }
    uint8_t left_height = __ttree_node_height(node->left);
    uint8_t right_height = __ttree_node_height(node->right);
    node->height = 1 + __max2(left_height, right_height);
}

// The balance factor of the node.
// When -1 <= balance factor <= 1, the node's subtree is balanced.
//
// Also see `__ttree_node_height`.
static int8_t __ttree_node_balance_factor(TTreeNode* node) {
    // NULL node is an empty subtree
    if (node == NULL) {
        return 0;
    }
    return __ttree_node_height(node->left) - __ttree_node_height(node->right);
}

// The maximum number of items that can be removed from this node
// without violating T-tree invariants.
//
// Also see `__ttree_node_move`.
static uint8_t __ttree_node_removable_count(TTreeNode* node) {
    if (__ttree_node_kind(node) == TTREE_NODEKIND_INTERNAL) {
        return node->length - TTREE_NODE_MIN_LEN;
    }
    bool can_subtract_one = node->length > 0;
    return node->length - can_subtract_one;
}

// Insert an ID and Row into the node at position `pos`.
// Appending can be achieved by setting `pos` to `node->length`.
// `pos` must be less than or equal to `node->length`.
//
// Also see `__ttree_node_remove`.
static void __ttree_node_insert(TTreeNode* node, uint8_t pos, ID id, Row row) {
    // pos == node->length is valid as it appends id and row to the end
    assert(pos <= node->length);
    assert(node->length < TTREE_NODE_SIZE);

    memmove(&node->ids[pos + 1], &node->ids[pos], (node->length - pos) * sizeof(ID));
    node->ids[pos] = id;
    memmove(&node->data[pos + 1], &node->data[pos], (node->length - pos) * sizeof(Row));
    node->data[pos] = row;
    node->last_id = node->ids[node->length];
    node->length++;
}

// Remove the ID and Row at position `pos` from the node, writing them to `out_id` and `out_row`.
// `pos` must be less than `node->length`.
//
// Also see `__ttree_node_insert`.
static void __ttree_node_remove(TTreeNode* node, uint8_t pos, ID* out_id, Row* out_row) {
    assert(pos < node->length);
    assert(node->length > 0);

    node->length--;
    *out_id = node->ids[pos];
    memmove(&node->ids[pos], &node->ids[pos + 1], (node->length - pos) * sizeof(ID));
    *out_row = node->data[pos];
    memmove(&node->data[pos], &node->data[pos + 1], (node->length - pos) * sizeof(Row));
    if (node->length > 0) {
        node->last_id = node->ids[node->length - 1];
    }
}

// Does `__ttree_node_remove(node, 0, out_id, out_row)` followed by
// `__ttree_node_insert(node, pos-1, id, row)`.
// This is faster than what the compiler generates for the above code. :(
// `pos` must be greater than 0 and less than `node->length`.
//
// Also see `__ttree_node_insert` and `__ttree_node_remove`.
static void __ttree_node_insert_removing_first(TTreeNode* node, uint8_t pos, ID id, Row row,
                                               ID* out_id, Row* out_row) {
    assert(0 < pos && pos < node->length);

    *out_id = node->ids[0];
    memmove(&node->ids[0], &node->ids[1], (pos - 1) * sizeof(ID));
    node->ids[pos - 1] = id;
    *out_row = node->data[0];
    memmove(&node->data[0], &node->data[1], (pos - 1) * sizeof(Row));
    node->data[pos - 1] = row;
    // Since we asserted that pos < node->length,
    // id cannot be the last ID and last_id remains unchanged.
    //
    // Length has not changed, no need to update it
}

// Copies `len` consecutive IDs and Rows from `src` starting at `src_start` to
// `dst` starting at `dst_start`. Removes the copied IDs and rows from `src`.
//
// `dst` and `src` must not be the same node.
static void __ttree_node_move(TTreeNode* dst, uint8_t dst_start, TTreeNode* src, uint8_t src_start,
                              uint8_t len) {
    assert(dst != src);
    assert(dst_start <= dst->length);
    assert(dst_start + len <= TTREE_NODE_SIZE);
    assert(src_start + len <= src->length);

    // Make space for the IDs and Rows to be copied to `dst`
    memmove(&dst->ids[dst_start + len], &dst->ids[dst_start],
            (dst->length - dst_start) * sizeof(ID));
    memmove(&dst->data[dst_start + len], &dst->data[dst_start],
            (dst->length - dst_start) * sizeof(Row));
    // Copy the IDs and Rows to `dst`
    memcpy(&dst->ids[dst_start], &src->ids[src_start], len * sizeof(ID));
    memcpy(&dst->data[dst_start], &src->data[src_start], len * sizeof(Row));
    dst->length += len;
    dst->last_id = dst->ids[dst->length - 1];
    // Remove the copied IDs and Rows from `src`
    memmove(&src->ids[src_start], &src->ids[src_start + len],
            (src->length - src_start - len) * sizeof(ID));
    memmove(&src->data[src_start], &src->data[src_start + len],
            (src->length - (src_start + len)) * sizeof(Row));
    src->length -= len;
    if (src->length > 0) {
        src->last_id = src->ids[src->length - 1];
    }
}

// Merge `right` into `left`, freeing `right`.
// All IDs in `left` must be less than those in `right`.
// The combined length must not exceed TTREE_NODE_SIZE.
// `left` and `right` must not be the same node.
static void __ttree_node_merge(TTreeNodeAllocator* allocator, TTreeNode* left, TTreeNode* right) {
    assert(left != right);
    assert(left->last_id < right->ids[0]);
    assert(left->length + right->length <= TTREE_NODE_SIZE);

    memcpy(&left->ids[left->length], &right->ids[0], right->length * sizeof(ID));
    memcpy(&left->data[left->length], &right->data[0], right->length * sizeof(Row));
    left->length += right->length;
    left->last_id = left->ids[left->length - 1];
    TTreeNodeAllocator_free(allocator, right);
}

// Create an empty TTree.
//
// The TTree must be destroyed with `TTree_destroy` to free memory.
TTree TTree_create(void) {
    return (TTree){
        .root = NULL,
        .node_allocator = NULL,
    };
}

// Recursively free all nodes in the node's subtree.
static void __destroy_inner(TTreeNodeAllocator* allocator, TTreeNode* node) {
    if (node == NULL) {
        return;
    }

    __destroy_inner(allocator, node->left);
    __destroy_inner(allocator, node->right);
    for (size_t i = 0; i < node->length; i++) {
        Row_destroy(&node->data[i]);
    }
    TTreeNodeAllocator_free(allocator, node);
}

// Remove and free all rows. The T-tree may be reused after calling this.
void TTree_destroy(TTree* tree) {
    if (tree == NULL) {
        return;
    }
    if (tree->node_allocator != NULL) {
        __destroy_inner(tree->node_allocator, tree->root);
        TTreeNodeAllocator_destroy(tree->node_allocator);
    }
    *tree = TTree_create();
}

// Perform a left rotation on `node`.
// Returns the new node at that took the place of `node`.
//
// See https://en.wikipedia.org/wiki/Tree_rotation#Illustration
static TTreeNode* __leftRotate(TTreeNode* node) {
    assert(node != NULL);
    assert(node->right != NULL);
    TTreeNode* right = node->right;
    TTreeNode* right_left = right->left;
    right->left = node;
    node->right = right_left;
    __ttree_node_update_height(node);
    __ttree_node_update_height(right);
    return right;
}

// Perform a right rotation on `node`.
// Returns the new node at that took the place of `node`.
//
// See https://en.wikipedia.org/wiki/Tree_rotation#Illustration
static TTreeNode* __rightRotate(TTreeNode* node) {
    assert(node != NULL);
    assert(node->left != NULL);
    TTreeNode* left = node->left;
    TTreeNode* left_right = left->right;
    left->right = node;
    node->left = left_right;
    __ttree_node_update_height(node);
    __ttree_node_update_height(left);
    return left;
}

// Rebalances the subtree rooted at `node` if it is unbalanced,
// maintaining the left-to-right order of nodes.
// Returns the new node at that took the place of `node`.
//
// Wikipedia has a good visual explanation:
// https://en.wikipedia.org/wiki/AVL_tree#Rebalancing
static TTreeNode* __rebalance_subtree(TTreeNode* node) {
    assert(node != NULL);
    if (__ttree_node_balance_factor(node) > 1) {
        if (__ttree_node_balance_factor(node->left) >= 0) {
            // Left left case
            return __rightRotate(node);
        } else {
            // Left right case
            node->left = __leftRotate(node->left);
            return __rightRotate(node);
        }
    }
    if (__ttree_node_balance_factor(node) < -1) {
        if (__ttree_node_balance_factor(node->right) <= 0) {
            // Right right case
            return __leftRotate(node);
        } else {
            // Right left case
            node->right = __rightRotate(node->right);
            return __leftRotate(node);
        }
    }
    // Balance factor is within [-1, 1], no balancing needed
    return node;
}

// Pop the last node from the node trace and get the parent's pointer to it.
static TTreeNode** __pop_node_get_pointer(TTree* tree, TTreeNodeList* node_trace) {
    TTreeNode* node = TTreeNodeList_pop(node_trace);
    TTreeNode* parent =
        node_trace->length == 0 ? NULL : TTreeNodeList_get(node_trace, node_trace->length - 1);
    TTreeNode** node_ptr = parent == NULL         ? &tree->root
                           : node == parent->left ? &parent->left
                                                  : &parent->right;
    assert(*node_ptr == node);
    return node_ptr;
}

// Ensure that internal nodes have at least TTREE_NODE_MIN_LEN items after rebalancing.
// Does nothing if `node` is not an internal node.
static void __ensure_min_len_after_rebalance(TTreeNode* node) {
    assert(node != NULL);
    // Make sure internal nodes have at least TTREE_NODE_MIN_LEN items
    if (__ttree_node_kind(node) != TTREE_NODEKIND_INTERNAL || node->length >= TTREE_NODE_MIN_LEN) {
        return;
    }

    // Otherwise, steal items from children
    uint8_t needed_count = TTREE_NODE_MIN_LEN - node->length;
    // Try left child
    uint8_t steal_count = __min2(needed_count, __ttree_node_removable_count(node->left));
    __ttree_node_move(node, 0, node->left, node->left->length - steal_count, steal_count);
    needed_count -= steal_count;
    // Try right child
    steal_count = __min2(needed_count, __ttree_node_removable_count(node->right));
    __ttree_node_move(node, node->length, node->right, 0, steal_count);
    needed_count -= steal_count;

    assert(node->length >= TTREE_NODE_MIN_LEN);
}

// Backtrack the node trace to rebalance the tree bottom-up.
// This function must be called after inserting or removing a node.
//
// Also see `__get_bounding_node`.
static void __rebalance_from_node_trace(TTree* tree, TTreeNodeList* node_trace) {
    while (node_trace->length > 0) {
        TTreeNode** node_ptr = __pop_node_get_pointer(tree, node_trace);
        TTreeNode* node = *node_ptr;
        __ttree_node_update_height(node);
        *node_ptr = __rebalance_subtree(node);
        if (*node_ptr != node) {
            // Only one rebalance is needed to rebalance the whole tree
            __ensure_min_len_after_rebalance(*node_ptr);
            break;
        }
    }
}

// Linear search for the position to insert `id` into the sorted array `ids` of length `ids_len`.
// For a TTREE_NODE_SIZE of 38, this is slightly faster than binary search.
//
// This function has O(ids_len) time complexity and O(1) space complexity.
static size_t __id_linear_search(ID* ids, size_t ids_len, ID id) {
    // Perhaps the compiler already does it, but manually using SIMD instructions
    // seems to make no performance difference.
    size_t pos = 0;
    while (pos < ids_len && id > ids[pos]) {
        pos++;
    }
    return pos;
}

// Finds the node in `tree` such that `id` is between the smallest and largest IDs in that node
// (called the "bounding node"). If no such node exists, the search stops at the last node visited.
// `out_pos` is set to the position within that node where `id` would be inserted.
//
// Appends the traversed nodes to `out_nodes`, with the first node being the root and the last node
// being the bounding node. `out_nodes` must have enough capacity to hold the traversed nodes.
// Asserts that the tree is not empty.
//
// Returns whether the ID already exists.
//
// This function has O(log(n)) time complexity and O(1) space complexity,
// where n is the number of nodes in `tree`.
static bool __get_bounding_node(const TTree* tree, ID id, TTreeNodeList* out_nodes,
                                size_t* out_pos) {
    assert(tree->root != NULL);

    // Search for bounding node, starting at root
    TTreeNode* node = tree->root;
    while (node != NULL) {
        TTreeNodeList_append_assume_capacity(out_nodes, node);
        size_t node_len = node->length;
        assert(node_len > 0);
        // id is too big, go right
        if (id > node->last_id) {
            node = node->right;
            continue;
        }
        // id is too small, go left
        if (id < node->ids[0]) {
            node = node->left;
            continue;
        }

        // id is bounded by this node, linear search for position
        size_t pos = __id_linear_search(node->ids, node_len, id);
        *out_pos = pos;
        return id == node->ids[pos];
    }

    // No bounding node, insert it to the left or right side of the last node visited
    assert(out_nodes->length > 0);
    TTreeNode* last_node = TTreeNodeList_get(out_nodes, out_nodes->length - 1);
    bool insert_left = id < last_node->ids[0];
    *out_pos = insert_left ? 0 : last_node->length;
    return false;
}

// Get a row by ID. Returns NULL if not found.
//
// This function has O(log(n)) time complexity and O(log(n)) space complexity,
// where n is the number of nodes in `tree`.
Row* TTree_get(const TTree* tree, ID id) {
    // Empty tree contains nothing
    if (tree->root == NULL) {
        return NULL;
    }

    TTreeNode* node_trace_buf[TTREE_NODE_TRACE_LEN];
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(node_trace_buf, TTREE_NODE_TRACE_LEN);
    size_t pos;
    bool exists = __get_bounding_node(tree, id, &node_trace, &pos);
    if (exists) {
        TTreeNode* node = TTreeNodeList_get(&node_trace, node_trace.length - 1);
        return &node->data[pos];
    }
    return NULL;
}

// Insert a row by ID. Pointers in `row` are copied and do not need to be retained.
// If the ID already exists, `tree` is not updated and this function returns false.
// Otherwise, returns true.
//
// This function has O(log(n)) time complexity and O(log(n)) space complexity,
// where n is the number of nodes in `tree`.
bool TTree_insert(TTree* tree, ID id, const Row* row) {
    if (tree->node_allocator == NULL) {
        tree->node_allocator = TTreeNodeAllocator_create();
    }
    if (tree->root == NULL) {
        tree->root = __ttree_node_create(tree->node_allocator);
        __ttree_node_insert(tree->root, 0, id, Row_dupe(row));
        return true;
    }

    // Find the node to insert into
    TTreeNode* node_trace_buf[TTREE_NODE_TRACE_LEN];
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(node_trace_buf, TTREE_NODE_TRACE_LEN);
    size_t pos;
    bool exists = __get_bounding_node(tree, id, &node_trace, &pos);
    TTreeNode* node = TTreeNodeList_get(&node_trace, node_trace.length - 1);
    if (exists) {
        return false;
    }

    // Insert row into node
    size_t node_len = node->length;
    assert(node_len > 0);
    if (node_len < TTREE_NODE_SIZE) {
        // There's space, insert it here
        __ttree_node_insert(node, pos, id, Row_dupe(row));
        return true;
    }

    // No more space, create a new node if id is out of range
    if (pos == 0 || pos == TTREE_NODE_SIZE) {
        TTreeNode** node_ptr = pos == 0 ? &node->left : &node->right;
        assert(*node_ptr == NULL);
        *node_ptr = __ttree_node_create(tree->node_allocator);
        __ttree_node_insert(*node_ptr, 0, id, Row_dupe(row));
        __rebalance_from_node_trace(tree, &node_trace);
        return true;
    }

    // No more space, displace the smallest ID
    ID removed_id;
    Row removed_row;
    __ttree_node_insert_removing_first(node, pos, id, Row_dupe(row), &removed_id, &removed_row);

    // Insert the removed id into the left subtree
    if (node->left == NULL) {
        node->left = __ttree_node_create(tree->node_allocator);
    }
    TTreeNode* child = node->left;
    while (child->right != NULL) {
        TTreeNodeList_append_assume_capacity(&node_trace, child);
        child = child->right;
    }
    if (child->length < TTREE_NODE_SIZE) {
        // We have space, insert it
        __ttree_node_insert(child, child->length, removed_id, removed_row);
        if (child->length > 1) {
            // No new node created, no need to rebalance
            return true;
        }
    } else {
        // There is no space in the left subtree, insert it further down
        assert(child->right == NULL);
        child->right = __ttree_node_create(tree->node_allocator);
        __ttree_node_insert(child->right, 0, removed_id, removed_row);
        // Child was already balanced, we don't need to balance it again
        assert(__ttree_node_height(child->left) <= 1);
        __ttree_node_update_height(child);
    }

    __rebalance_from_node_trace(tree, &node_trace);
    return true;
}

// Rebalance the subtree after a row was removed.
// `node_ptr` must point to a non-internal node.
// Return whether a node was deleted.
//
// Also see `__rebalance_after_remove_internal`.
static bool __rebalance_after_remove_non_internal(TTreeNodeAllocator* allocator,
                                                  TTreeNode** node_ptr) {
    TTreeNode* node = *node_ptr;
    assert(node != NULL);
    assert(__ttree_node_kind(node) != TTREE_NODEKIND_INTERNAL);

    // Delete leaf if empty
    if (__ttree_node_kind(node) == TTREE_NODEKIND_LEAF) {
        if (node->length == 0) {
            TTreeNodeAllocator_free(allocator, node);
            *node_ptr = NULL;
            return true;
        }
        return false;
    }

    // Half-leaf node case, try to merge with child
    TTreeNode* child = node->left != NULL ? node->left : node->right;
    // Balance factor cannot exceed +-1, so child must be a leaf node
    assert(__ttree_node_kind(child) == TTREE_NODEKIND_LEAF);
    if (node->length + child->length > TTREE_NODE_SIZE) {
        // Not enough space to merge
        return false;
    }
    // Merge child into node
    if (child == node->left) {
        // Make node the one with the smaller IDs
        *node_ptr = child;
        child = node;
        node = *node_ptr;
    }
    __ttree_node_merge(allocator, node, child);

    // We deleted the only child, this is now a leaf node
    node->left = NULL;
    node->right = NULL;

    __ttree_node_update_height(node);
    *node_ptr = __rebalance_subtree(node);
    return true;
}

// Rebalance the subtree after a row was removed.
// `node` must be an internal node.
// Returns whether a node was deleted.
//
// Also see `__rebalance_after_remove_non_internal`.
static bool __rebalance_after_remove_internal(TTreeNodeAllocator* allocator, TTreeNode* node,
                                              TTreeNodeList* node_trace) {
    assert(node != NULL);
    assert(__ttree_node_kind(node) == TTREE_NODEKIND_INTERNAL);
    // Ensure min length
    if (node->length >= TTREE_NODE_MIN_LEN) {
        return false;
    }

    // Otherwise, steal the largest ID from the left subtree
    TTreeNode* child = node->left;
    size_t subtree_start = node_trace->length;
    while (child->right != NULL) {
        TTreeNodeList_append_assume_capacity(node_trace, child);
        child = child->right;
    }

    ID removed_id;
    Row removed_row;
    __ttree_node_remove(child, child->length - 1, &removed_id, &removed_row);
    __ttree_node_insert(node, 0, removed_id, removed_row);

    // Rebalance the child
    TTreeNode** child_ptr = node_trace->length == subtree_start
                                ? &node->left
                                : &TTreeNodeList_get(node_trace, node_trace->length - 1)->right;
    return __rebalance_after_remove_non_internal(allocator, child_ptr);
}

// Remove a row by ID. Returns whether a value was removed.
//
// This function has O(log(n)) time complexity and O(log(n)) space complexity,
// where n is the number of nodes in `tree`.
bool TTree_remove(TTree* tree, ID id) {
    if (tree->root == NULL) {
        return false;
    }

    // Find the node to remove from
    TTreeNode* node_trace_buf[TTREE_NODE_TRACE_LEN];
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(node_trace_buf, TTREE_NODE_TRACE_LEN);
    size_t pos;
    bool exists = __get_bounding_node(tree, id, &node_trace, &pos);
    TTreeNode* node = TTreeNodeList_get(&node_trace, node_trace.length - 1);
    if (!exists) {
        return false;
    }

    // Remove the id and row
    {
        ID removed_id;
        Row removed_row;
        __ttree_node_remove(node, pos, &removed_id, &removed_row);
        Row_destroy(&removed_row);
    }

    // Rebalance the tree
    bool node_deleted;
    if (__ttree_node_kind(node) == TTREE_NODEKIND_INTERNAL) {
        node_deleted = __rebalance_after_remove_internal(tree->node_allocator, node, &node_trace);
    } else {
        TTreeNode** node_ptr = __pop_node_get_pointer(tree, &node_trace);
        node_deleted = __rebalance_after_remove_non_internal(tree->node_allocator, node_ptr);
    }
    if (node_deleted) {
        __rebalance_from_node_trace(tree, &node_trace);
    }
    return true;
}

// See `TTree_iter_start` and `TTree_iter_next`.
typedef struct {
    // Stack of nodes from the root to the current node.
    TTreeNode* node_trace[TTREE_NODE_TRACE_LEN];
    // Number of nodes in `node_trace`.
    uint8_t node_trace_len;
    // Position within the current node.
    uint8_t pos;
} TTreeIter;

// Create an iterator starting at the smallest ID in the tree.
// Use `TTree_iter_next` to advance the iterator.
// Values are iterated in ascending order of ID.
// The iterator is invalidated if the tree is modified.
TTreeIter TTree_iter_start(const TTree* tree) {
    TTreeIter iter;
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(iter.node_trace, TTREE_NODE_TRACE_LEN);
    TTreeNode* node = tree->root;
    // Smallest ID is all the way to the left
    while (node != NULL) {
        TTreeNodeList_append_assume_capacity(&node_trace, node);
        node = node->left;
    }
    iter.node_trace_len = node_trace.length;
    iter.pos = 0;
    return iter;
}

// Advance the iterator and write the next ID and Row to `out_id` and `out_row`.
// Returns whether there was a next ID and Row.
//
// Also see `TTree_iter_start`.
bool TTree_iter_next(TTreeIter* iter, ID* out_id, Row** out_row) {
    if (iter->node_trace_len == 0) {
        return false;
    }
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(iter->node_trace, TTREE_NODE_TRACE_LEN);
    node_trace.length = iter->node_trace_len;

    // Go to the next position in the current node
    TTreeNode* node = TTreeNodeList_get(&node_trace, node_trace.length - 1);
    *out_id = node->ids[iter->pos];
    *out_row = &node->data[iter->pos];
    if (++iter->pos < node->length) {
        return true;
    }

    // Finished this node, go to the next node
    iter->pos = 0;
    // If right subtree exists, it contains the next largest ID
    if (node->right != NULL) {
        node = node->right;
        while (node != NULL) {
            TTreeNodeList_append(&node_trace, node);
            node = node->left;
        }
        iter->node_trace_len = node_trace.length;
        return true;
    }

    // Otherwise, go up until we find a node where we came from the left subtree.
    // That node's subtree will contain the next largest ID.
    while (true) {
        TTreeNode* node = TTreeNodeList_pop(&node_trace);
        // We iterated all the nodes
        if (node_trace.length == 0) {
            break;
        }
        TTreeNode* parent = TTreeNodeList_get(&node_trace, node_trace.length - 1);
        if (parent->left == node) {
            // Left subtree finished, the parent contains the next largest ID.
            // Conveniently the parent is already the last item in the list.
            break;
        }
        // Right subtree finished, meaning the parent's subtree is also finished
        // and we need to go up again
    }
    iter->node_trace_len = node_trace.length;
    return true;
}

// Contains the state of a bulk insert operation.
typedef struct {
    // The current node being filled.
    TTreeNode* current;
    // The TTree being constructed.
    TTree tree;
} TTreeBulkInsert;

// Begin a bulk insert operation on a new TTree.
// IDs must be inserted in strictly ascending order.
// This is faster than calling `TTree_insert` in a loop.
//
// Insert IDs and rows using `TTree_bulk_insert`.
// `TTree_bulk_insert_end` must be called after all inserts are done.
TTreeBulkInsert TTree_bulk_insert_start(void) {
    return (TTreeBulkInsert){
        .current = NULL,
        .tree = TTree_create(),
    };
}

// Should be called during a bulk insert when a node is full or is the last node.
// Updates the node's last_id and inserts it into the tree, balancing as necessary.
static void __bulk_insert_finish_node(TTreeNode* node, TTree* tree) {
    assert(node != NULL);
    assert(node->length > 0);
    node->last_id = node->ids[node->length - 1];
    if (tree->root == NULL) {
        tree->root = node;
        return;
    }

    TTreeNode* node_trace_buf[TTREE_NODE_TRACE_LEN];
    TTreeNodeList node_trace = TTreeNodeList_from_buffer(node_trace_buf, TTREE_NODE_TRACE_LEN);
    // Get the node with the largest ID, which is all the way to the right
    TTreeNode* trace = tree->root;
    while (trace != NULL) {
        TTreeNodeList_append_assume_capacity(&node_trace, trace);
        trace = trace->right;
    }

    TTreeNode* parent = TTreeNodeList_get(&node_trace, node_trace.length - 1);
    assert(parent->last_id < node->ids[0]);
    parent->right = node;
    __rebalance_from_node_trace(tree, &node_trace);
}

// Insert a new id and row as part of a bulk insert operation.
// `id` must be larger than all previously inserted IDs.
//
// Also see `TTree_bulk_insert_start`.
void TTree_bulk_insert(TTreeBulkInsert* bulk, ID id, Row* row) {
    if (bulk->tree.node_allocator == NULL) {
        bulk->tree.node_allocator = TTreeNodeAllocator_create();
    }
    if (bulk->current == NULL) {
        bulk->current = __ttree_node_create(bulk->tree.node_allocator);
        __ttree_node_insert(bulk->current, 0, id, Row_dupe(row));
        return;
    }

    TTreeNode* node = bulk->current;
    assert(node->ids[node->length - 1] < id);
    // Insert the id and row into the node
    // Don't use __ttree_node_insert as we can defer updating the last_id
    node->ids[node->length] = id;
    node->data[node->length] = Row_dupe(row);
    node->length++;
    // If node is full, finish it and create a new node
    if (node->length == TTREE_NODE_SIZE) {
        __bulk_insert_finish_node(node, &bulk->tree);
        bulk->current = NULL;
    }
}

// Finish a bulk insert operation and return the resulting TTree.
//
// Also see `TTree_bulk_insert_start`.
TTree TTree_bulk_insert_end(TTreeBulkInsert* bulk) {
    if (bulk->current != NULL) {
        __bulk_insert_finish_node(bulk->current, &bulk->tree);
        bulk->current = NULL;
    }
    return bulk->tree;
}
