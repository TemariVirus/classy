// Concrete T-tree implementation that stores rows and IDs in ascending order.
// Some terminology copied from Wikipedia (https://en.wikipedia.org/wiki/T-tree):
// - An "internal node" has two children.
// - A "half-leaf node" has one child.
// - A "leaf node" has no children.
// - The "bounding node" for an ID is the node such that the ID is between the node's smallest and
//   largest IDs, inclusively.
//
// Also see Wikipedia's article on AVL trees (https://en.wikipedia.org/wiki/AVL_tree) as the T-tree
// article lacks detail.

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "row.h"

#define CACHE_ALIGN 64 // Assume cache line is 64B
#define NODE_SIZE 38   // Fastest node size found empirically
// Minimum number of items in an internal node.
// No significant performance difference found when tuning this.
#define NODE_MIN_LEN ((NODE_SIZE * 3 + 3) / 4)
// Since there are only 2^32 unique sutdent IDs, the tree will never have more than 38 levels.
// T-trees follow the same height bounds as AVL trees:
// https://en.wikipedia.org/wiki/AVL_tree#Properties
//
// log_phi((2^32 / NODE_MIN_LEN) + 2) + b = 38.768...
#define NODE_TRACE_SIZE 38

// A node in the T-tree. Contains up to NODE_SIZE IDs and Rows.
//
// ID is split from the rest of the data so that we can pack them more tightly in cache
typedef struct Node {
    // Left child
    struct Node* left;
    // Right child
    struct Node* right;
    // Number of IDs in this node.
    uint8_t length;
    // Height of the subtree rooted at this node.
    // i.e., how many levels are below this node, including the node itself.
    uint8_t height;
    // Copy of the last ID to reduce cache misses during search.
    ID last_id;
    // IDs must always be sorted in ascending order.
    ID ids[NODE_SIZE];
    // data[0] is associated with ids[0], etc.
    // data must come last so that everything else is cache-aligned.
    Row data[NODE_SIZE];
} Node;
// Ensure that Node.left, Node.right, Node.ids[0] and Node.last_id all lie on the same cache line.
// These 4 fields are used in searching for the bounding node (which takes up the majority of time),
// so this speeds things up considerably.
static_assert(sizeof(Node) % CACHE_ALIGN == 0, "Node size must be a multiple of cache line size");

// chunked-allocator for fast aligned allocation
#define TYPE Node
#define TYPED(THING) Node##THING
#include "chunked-allocator.h"

#define TYPE Node*
#define TYPED(THING) Node##THING
#include "list.h"

// The TTree owns the memory (and strings) of the nodes and data.
typedef struct {
    // NULL only if the tree is empty.
    Node* root;
    // NULL only if the tree is empty.
    NodeAllocator* node_allocator;
} TTree;

// Create an empty node.
static Node* __node_create(NodeAllocator* allocator) {
    Node* node = NodeAllocator_alloc(allocator);
    // Ensure the node is cache-aligned for performance reasons.
    assert((uintptr_t)node % CACHE_ALIGN == 0);
    node->left = NULL;
    node->right = NULL;
    node->length = 0;
    node->height = 1;
    return node;
}

// Get the height of the node's subtree.
//
// Also see `__update_node_height`.
static uint8_t __node_height(Node* node) {
    // NULL node is an empty subtree
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

// Update the height of the node's subtree. The height of the children must be correct.
//
// Also see `__node_height`.
static void __update_node_height(Node* node) {
    if (node == NULL) {
        return;
    }
    uint8_t left_height = __node_height(node->left);
    uint8_t right_height = __node_height(node->right);
    node->height = 1 + (left_height > right_height ? left_height : right_height);
}

// The balance factor of the node.
//
// Also see `__node_height`.
static int8_t __node_balance(Node* node) {
    // NULL node is an empty subtree
    if (node == NULL) {
        return 0;
    }
    return __node_height(node->left) - __node_height(node->right);
}

// Insert an ID and Row into the node at position `pos`.
// Appending can be achieved by setting `pos` to `node->length`.
//
// Also see `__node_remove`.
static void __node_insert(Node* node, uint8_t pos, ID id, Row row) {
    // pos == node->length is valid as it appends id and row to the end
    assert(pos <= node->length);
    assert(node->length < NODE_SIZE);

    memmove(&node->ids[pos + 1], &node->ids[pos], (node->length - pos) * sizeof(ID));
    node->ids[pos] = id;
    memmove(&node->data[pos + 1], &node->data[pos], (node->length - pos) * sizeof(Row));
    node->data[pos] = row;
    node->last_id = node->ids[node->length];
    node->length++;
}

// Remove the ID and Row at position `pos` from the node, writing them to `out_id` and `out_row`.
//
// Also see `__node_insert`.
static void __node_remove(Node* node, uint8_t pos, ID* out_id, Row* out_row) {
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

// Does `__node_remove(node, 0, out_id, out_row)` followed by `__node_insert(node, pos-1, id, row)`.
// Asserts that `pos > 0` and `pos < node->length`.
// This is faster than what the compiler generates for the above code :(
//
// Also see `__node_insert` and `__node_remove`.
static void __node_insert_removing_first(Node* node, uint8_t pos, ID id, Row row, ID* out_id,
                                         Row* out_row) {
    assert(pos > 0 && pos < node->length);

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

// Create an empty TTree.
//
// The TTree must be destroyed with `TTree_destroy` to free memory.
TTree TTree_create(void) {
    return (TTree){
        .root = NULL,
        .node_allocator = NULL,
    };
}

// Recursively free all nodes in node's subtree.
static void __destroy_inner(NodeAllocator* allocator, Node* node) {
    if (node == NULL) {
        return;
    }

    __destroy_inner(allocator, node->left);
    __destroy_inner(allocator, node->right);
    for (size_t i = 0; i < node->length; i++) {
        Row_destroy(&node->data[i]);
    }
    NodeAllocator_free(allocator, node);
}

// Remove and free all rows. The T-tree may be reused after calling this.
void TTree_destroy(TTree* tree) {
    if (tree == NULL) {
        return;
    }
    if (tree->node_allocator != NULL) {
        __destroy_inner(tree->node_allocator, tree->root);
        NodeAllocator_destroy(tree->node_allocator);
    }
    *tree = TTree_create();
}

// Perform a left rotation on `node`.
// Returns the new node at that took the place of `node`.
//
// See https://en.wikipedia.org/wiki/Tree_rotation#Illustration
static Node* __leftRotate(Node* node) {
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

// Perform a right rotation on `node`.
// Returns the new node at that took the place of `node`.
//
// See https://en.wikipedia.org/wiki/Tree_rotation#Illustration
static Node* __rightRotate(Node* node) {
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

// Rebalances the subtree rooted at `node`, maintaining the "horizontal" order of nodes.
// Returns the new node at that took the place of `node`.
//
// Wikipedia has a good visual explanation:
// https://en.wikipedia.org/wiki/AVL_tree#Rebalancing
static Node* __rebalance_subtree(Node* node) {
    assert(node != NULL);
    if (__node_balance(node) > 1) {
        if (__node_balance(node->left) >= 0) {
            // Left left case
            return __rightRotate(node);
        } else {
            // Left right case
            node->left = __leftRotate(node->left);
            return __rightRotate(node);
        }
    }
    if (__node_balance(node) < -1) {
        if (__node_balance(node->right) <= 0) {
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

// Linear search for the position to insert `id` into the sorted array `ids` of length `ids_len`.
// For a NODE_SIZE of 38, this appears to be slightly faster than binary search.
static size_t __linear_search(ID* ids, size_t ids_len, ID id) {
    // Perhaps the compiler already does it, but manually using SIMD instructions
    // seems to make no performance difference.
    size_t pos = 0;
    while (pos < ids_len && id > ids[pos]) {
        pos++;
    }
    return pos;
}

// Sets `out_pos` to the position within the bounding node used for insertion of the given ID.
// Appends the traversed nodes to `out_nodes`, with the first node being the root and the last node
// being the bounding node. `out_nodes` must have enough capacity to hold the traversed nodes.
// Asserts that the tree is not empty.
//
// Returns whether the ID already exists.
static bool __get_bounding(const TTree* tree, ID id, NodeList* out_nodes, size_t* out_pos) {
    assert(tree->root != NULL);

    // Search for bounding node, starting at root
    Node* node = tree->root;
    while (node != NULL) {
        NodeList_append_assume_capacity(out_nodes, node);
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
        size_t pos = __linear_search(node->ids, node_len, id);
        *out_pos = pos;
        return id == node->ids[pos];
    }

    // No bounding node, insert it to the left or right of the last node visited
    assert(out_nodes->length > 0);
    Node* last_node = NodeList_get(out_nodes, out_nodes->length - 1);
    bool insert_left = id < last_node->ids[0];
    *out_pos = insert_left ? 0 : last_node->length;
    return false;
}

// Backtrack the node trace to re-balance the tree bottom-up.
//
// Asserts that `node_trace` is not empty.
static void __rebalance_from_node_trace(TTree* tree, NodeList* node_trace) {
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
//
// This function has O(log(n)) time complexity and O(1) space complexity,
// where n is the number of nodes in `tree`.
Row* TTree_get(const TTree* tree, ID id) {
    // Empty tree contains nothing
    if (tree->root == NULL) {
        return NULL;
    }

    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = NodeList_from_buffer(node_trace_buf, NODE_TRACE_SIZE);
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    if (exists) {
        Node* node = NodeList_get(&node_trace, node_trace.length - 1);
        return &node->data[pos];
    }
    return NULL;
}

// Insert or update a row by ID. Pointers in `row` are copied and do not need to be retained.
//
// This function has O(log(n)) time complexity and O(1) space complexity,
// where n is the number of nodes in `tree`.
void TTree_put(TTree* tree, ID id, const Row* row) {
    if (tree->node_allocator == NULL) {
        tree->node_allocator = NodeAllocator_create();
    }
    if (tree->root == NULL) {
        tree->root = __node_create(tree->node_allocator);
        __node_insert(tree->root, 0, id, Row_dupe(row));
        return;
    }

    // Find the node to insert into
    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = NodeList_from_buffer(node_trace_buf, NODE_TRACE_SIZE);
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    Node* node = NodeList_get(&node_trace, node_trace.length - 1);
    if (exists) {
        // Update existing row
        Row_destroy(&node->data[pos]);
        node->data[pos] = Row_dupe(row);
        return;
    }

    // Insert row into node
    size_t node_len = node->length;
    assert(node_len > 0);
    if (node_len < NODE_SIZE) {
        // There's space, insert it here
        __node_insert(node, pos, id, Row_dupe(row));
        return;
    }

    // No more space, create a new node if id is out of range
    if (pos == 0) {
        assert(node->left == NULL);
        node->left = __node_create(tree->node_allocator);
        __node_insert(node->left, 0, id, Row_dupe(row));
        __rebalance_from_node_trace(tree, &node_trace);
        return;
    }
    if (pos == NODE_SIZE) {
        assert(node->right == NULL);
        node->right = __node_create(tree->node_allocator);
        __node_insert(node->right, 0, id, Row_dupe(row));
        __rebalance_from_node_trace(tree, &node_trace);
        return;
    }

    // No more space, displace the smallest ID
    ID removed_id;
    Row removed_row;
    __node_insert_removing_first(node, pos, id, Row_dupe(row), &removed_id, &removed_row);

    // Insert the removed id into the left subtree
    if (node->left == NULL) {
        node->left = __node_create(tree->node_allocator);
    }
    Node* child = node->left;
    while (child->right != NULL) {
        NodeList_append_assume_capacity(&node_trace, child);
        child = child->right;
    }

    if (child->length < NODE_SIZE) {
        // We have space, insert it
        __node_insert(child, child->length, removed_id, removed_row);
        if (child->length > 1) {
            // No new node created, no need to rebalance
            return;
        }
    } else {
        // There is no space in the left subtree, insert it further down
        assert(child->right == NULL);
        child->right = __node_create(tree->node_allocator);
        __node_insert(child->right, 0, removed_id, removed_row);
        // Child was already balanced, we don't need to balance it again
        assert(__node_height(child->left) <= 1);
        __update_node_height(child);
    }

    __rebalance_from_node_trace(tree, &node_trace);
}

// Rebalance the subtree after a row was removed. `node_ptr` must point to a non-internal
// (i.e., leaf or half-leaf) node. Return whether a node was deleted.
static bool __rebalance_after_remove_non_internal(NodeAllocator* allocator, Node** node_ptr) {
    Node* node = *node_ptr;
    assert(node != NULL);
    assert(node->left == NULL || node->right == NULL);

    if (node->left == NULL && node->right == NULL) {
        // Leaf node case, delete if empty
        if (node->length == 0) {
            NodeAllocator_free(allocator, node);
            *node_ptr = NULL;
            return true;
        }
        return false;
    }

    // Half-leaf node case, try to merge with child
    Node* child = node->left != NULL ? node->left : node->right;
    // Balance factor cannot exceed +-1, so child must be a leaf node
    assert(child->left == NULL && child->right == NULL);
    if (node->length + child->length > NODE_SIZE) {
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
    memcpy(&node->ids[node->length], &child->ids[0], child->length * sizeof(ID));
    memcpy(&node->data[node->length], &child->data[0], child->length * sizeof(Row));
    node->length += child->length;
    node->last_id = node->ids[node->length - 1];

    // We deleted the only child, this is now a leaf node
    node->left = NULL;
    node->right = NULL;
    NodeAllocator_free(allocator, child);

    __update_node_height(node);
    *node_ptr = __rebalance_subtree(node);
    return true;
}

// Remove a row by ID. Returns whether a value was removed.
bool TTree_remove(TTree* tree, ID id) {
    if (tree->root == NULL) {
        return false;
    }

    // Find the node to remove from
    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = NodeList_from_buffer(node_trace_buf, NODE_TRACE_SIZE);
    size_t pos;
    bool exists = __get_bounding(tree, id, &node_trace, &pos);
    Node* node = NodeList_get(&node_trace, node_trace.length - 1);
    if (!exists) {
        return false;
    }

    // Remove the id and row
    {
        ID removed_id;
        Row removed_row;
        __node_remove(node, pos, &removed_id, &removed_row);
        Row_destroy(&removed_row);
    }

    // Rebalance the tree
    if (node->left == NULL || node->right == NULL) {
        // Half-leaf or leaf node case
        if (node_trace.length <= 1) {
            // There is only a root node
            __rebalance_after_remove_non_internal(tree->node_allocator, &tree->root);
            return true;
        }

        node = NodeList_pop(&node_trace);
        Node* parent = NodeList_get(&node_trace, node_trace.length - 1);
        Node** node_ptr = node == parent->left ? &parent->left : &parent->right;
        bool deleted = __rebalance_after_remove_non_internal(tree->node_allocator, node_ptr);
        node = parent;
        if (!deleted) {
            return true;
        }
        goto rebalance;
    }

    // Internal node case, ensure min length
    if (node->length >= NODE_MIN_LEN) {
        return true;
    }

    // Otherwise, steal the smallest ID from the right subtree
    Node* child = node->right;
    size_t subtree_start = node_trace.length;
    while (child->left != NULL) {
        NodeList_append_assume_capacity(&node_trace, child);
        child = child->left;
    }

    ID removed_id;
    Row removed_row;
    assert(child->length > 0);
    __node_remove(child, 0, &removed_id, &removed_row);
    __node_insert(node, node->length, removed_id, removed_row);

    // Rebalance the child
    Node** child_ptr = node_trace.length == subtree_start
                           ? &node->right
                           : &NodeList_get(&node_trace, node_trace.length - 1)->left;
    bool deleted = __rebalance_after_remove_non_internal(tree->node_allocator, child_ptr);
    if (!deleted) {
        return true;
    }

rebalance:
    __rebalance_from_node_trace(tree, &node_trace);
    return true;
}

// See `TTree_iter_start` and `TTree_iter_next`.
typedef struct {
    NodeList nodes;
    uint8_t pos;
} TTreeIter;

// Create an iterator starting at the beginning of the TTree.
// Use TTree_iter_next to advance the iterator.
// Values are iterated in ascending order of ID.
// The iterator is invalidated if the TTree is modified.
TTreeIter TTree_iter_start(const TTree* tree) {
    NodeList nodes = NodeList_create();
    Node* node = tree->root;
    // Smallest ID is all the way to the left
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

    // Go to the next position in the current node
    Node* node = NodeList_get(&iter->nodes, iter->nodes.length - 1);
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
            NodeList_append(&iter->nodes, node);
            node = node->left;
        }
        return true;
    }

    // Otherwise, go up until we find a node where we came from the left subtree.
    // That node's subtree will contain the next largest ID.
    while (true) {
        Node* node = NodeList_pop(&iter->nodes);
        // We iterated all the nodes
        if (iter->nodes.length == 0) {
            break;
        }
        Node* parent = NodeList_get(&iter->nodes, iter->nodes.length - 1);
        if (parent->left == node) {
            // Left subtree finished, the parent contains the next largest ID.
            // Conveniently the parent is already the last item in the list.
            break;
        }
        // Right subtree finished, meaning the parent's subtree is also finished
        // and we need to go up again
    }
    return true;
}

// Contains the state of a bulk insert operation.
typedef struct {
    Node* current;
    TTree tree;
} TTreeBulkInsert;

// Begin a bulk insert operation on a new TTree.
// IDs must be inserted in strictly ascending order.
// This is faster than calling `TTree_put` in a loop.
//
// `TTree_bulk_insert_end` must be called after all inserts are done.
TTreeBulkInsert TTree_bulk_insert_start(void) {
    return (TTreeBulkInsert){
        .current = NULL,
        .tree = TTree_create(),
    };
}

// Should be called when a node is full or is the last node during a bulk insert.
//
// Updates the node's last_id and inserts it into the tree, balancing as necessary.
static void __bulk_insert_finish_node(Node* node, TTree* tree) {
    assert(node != NULL);
    assert(node->length > 0);
    node->last_id = node->ids[node->length - 1];
    if (tree->root == NULL) {
        tree->root = node;
        return;
    }

    Node* node_trace_buf[NODE_TRACE_SIZE];
    NodeList node_trace = NodeList_from_buffer(node_trace_buf, NODE_TRACE_SIZE);
    // Get the node with the largest ID, which is all the way to the right
    Node* trace = tree->root;
    while (trace != NULL) {
        NodeList_append_assume_capacity(&node_trace, trace);
        trace = trace->right;
    }

    Node* parent = NodeList_get(&node_trace, node_trace.length - 1);
    assert(parent->last_id < node->ids[0]);
    parent->right = node;
    __rebalance_from_node_trace(tree, &node_trace);
}

// Insert a new id and row as part of a bulk insert operation.
// `id` must be larger than all previously inserted IDs.
void TTree_bulk_insert(TTreeBulkInsert* bulk, ID id, Row* row) {
    if (bulk->tree.node_allocator == NULL) {
        bulk->tree.node_allocator = NodeAllocator_create();
    }
    if (bulk->current == NULL) {
        bulk->current = __node_create(bulk->tree.node_allocator);
        __node_insert(bulk->current, 0, id, Row_dupe(row));
        return;
    }

    Node* node = bulk->current;
    assert(node->ids[node->length - 1] < id);
    // Insert the id and row into the node
    // Don't use __node_insert as we can defer updating the last_id
    node->ids[node->length] = id;
    node->data[node->length] = Row_dupe(row);
    node->length++;
    // If node is full, finish it and create a new node
    if (node->length == NODE_SIZE) {
        __bulk_insert_finish_node(node, &bulk->tree);
        bulk->current = NULL;
    }
}

// Finish a bulk insert operation and return the resulting TTree.
TTree TTree_bulk_insert_end(TTreeBulkInsert* bulk) {
    if (bulk->current != NULL) {
        __bulk_insert_finish_node(bulk->current, &bulk->tree);
        bulk->current = NULL;
    }
    return bulk->tree;
}
