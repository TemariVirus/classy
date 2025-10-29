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

// Split ID from the rest of the data so that we can pack them more tightly in cache
typedef struct Node {
    ID ids[NODE_SIZE];
    // Number of IDs in this node.
    uint8_t length;
    // Unless you have 5.79e64TB of RAM, a u8 is enough.
    uint8_t height;
    struct Node* left;
    struct Node* right;
    // TODO: add a parent pointer
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
    bool go_right_next;
} TTreeIter;

// Create an empty node.
Node* __create_node_empty() {
    // TODO: allocate in bigger blocks so that this outperforms malloc
    // Node* node = aligned_alloc(CACHE_SIZE, sizeof(Node));
    Node* node = malloc(sizeof(Node));
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
TTree TTree_create() { return (TTree){.root = NULL}; }

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

    // Use SIMD for first 8 comparisions if possible
#if defined(__AVX2__)
    // Add offset to convert to signed range for correct comparison
    const __m256i offset = _mm256_set1_epi32(INT32_MIN);
    __m256i id_vec = _mm256_set1_epi32(id + INT32_MIN);
    __m256i ids_vec = _mm256_loadu_si256((__m256i*)ids);
    ids_vec = _mm256_add_epi32(ids_vec, offset);
    __m256i cmp_mask = _mm256_cmpgt_epi32(id_vec, ids_vec);
    int mask = _mm256_movemask_ps((__m256)cmp_mask);
    pos += __builtin_ctz(~mask | (1 << 8));
#endif

    while (pos < ids_len && id > ids[pos]) {
        pos++;
    }
    return pos > ids_len ? ids_len : pos;
}

// Sets out_node and out_pos to the bounding node and
// the position within that node used for insertion of the given ID.
// Returns true if the ID already exists, false otherwise.
bool __get_bounding(const TTree* tree, ID id, Node** out_node, size_t* out_pos) {
    assert(tree->root != NULL);

    // Search for bounding node, starting at root
    Node* node = tree->root;
    Node* parent = NULL;
    while (node != NULL) {
        size_t node_len = node->length;
        assert(node_len > 0);
        // id is too small, go left
        if (id < node->ids[0]) {
            parent = node;
            node = node->left;
            continue;
        }
        // id is too big, go right
        if (id > node->ids[node_len - 1]) {
            parent = node;
            node = node->right;
            continue;
        }

        // TODO: optimize with SIMD
        // id is bounded by this node, linear scan for position
        size_t pos = 0;
        while (pos < node_len && id > node->ids[pos]) {
            pos++;
        }
        *out_node = node;
        *out_pos = pos;
        return id == node->ids[pos];
    }

    // No bounding node, try to insert it into the last node
    assert(parent != NULL);
    *out_node = parent;
    bool insert_left = id < parent->ids[0];
    *out_pos = insert_left ? 0 : parent->length;
    return false;
}

// Get a row by ID. Returns NULL if not found.
Row* TTree_get(const TTree* tree, ID id) {
    Node* node;
    size_t pos;
    bool exists = __get_bounding(tree, id, &node, &pos);
    if (exists) {
        return &node->data[pos];
    }
    return NULL;
}

// Insert or update a row by ID into the subtree rooted at `node`.
// Returns the new root of the subtree, or NULL if no node was created.
Node* __put_inner(Node* node, ID id, const Row* row) {
    assert(node != NULL);
    size_t node_len = node->length;
    assert(node_len > 0);
    if (node->left != NULL && id < node->ids[0]) {
        // id is too small, go left
        Node* new_left = __put_inner(node->left, id, row);
        if (new_left == NULL) {
            return NULL;
        }
        node->left = new_left;
    } else if (node->right != NULL && id > node->ids[node_len - 1]) {
        // id is too big, go right
        Node* new_right = __put_inner(node->right, id, row);
        if (new_right == NULL) {
            return NULL;
        }
        node->right = new_right;
    } else {
        // id is bounded by this node, insert it
        size_t pos = __linear_search(node->ids, node_len, id);
        if (pos < NODE_SIZE && id == node->ids[pos]) {
            // Update existing row
            Row_destroy(&node->data[pos]);
            node->data[pos] = Row_dupe(row);
            return NULL;
        }

        if (node_len < NODE_SIZE) {
            // There's space, insert it here
            memmove(&node->ids[pos + 1], &node->ids[pos], (node_len - pos) * sizeof(ID));
            node->ids[pos] = id;
            node->length++;
            // TODO: check performance diff when rows are pointers instead of the whole thing
            memmove(&node->data[pos + 1], &node->data[pos], (node_len - pos) * sizeof(Row));
            node->data[pos] = Row_dupe(row);
            return NULL;
        }

        // No more space, create a new node or displace values
        if (pos == 0) {
            assert(node->left == NULL);
            node->left = __create_node(id, row);
        } else if (pos == NODE_SIZE) {
            assert(node->right == NULL);
            node->right = __create_node(id, row);
        } else {
            // Remove smallest id
            ID removed_id = node->ids[0];
            memmove(&node->ids[0], &node->ids[1], (pos - 1) * sizeof(ID));
            node->ids[pos - 1] = id;
            // TODO: check performance diff when rows are pointers instead of the whole thing
            Row removed_row = node->data[0];
            memmove(&node->data[0], &node->data[1], (pos - 1) * sizeof(Row));
            node->data[pos - 1] = Row_dupe(row);

            // Insert the removed id into the left subtree
            if (node->left == NULL) {
                node->left = __create_node_empty();
            }
            Node* child = node->left;
            if (child->length < NODE_SIZE) {
                child->ids[child->length] = removed_id;
                child->data[child->length++] = removed_row;
                // No new node created, exit early
                if (child->length > 1) {
                    return NULL;
                }
            } else {
                // There is no space in the left node, put it further down
                if (child->right == NULL) {
                    child->right = __create_node(removed_id, &removed_row);
                    __update_node_height(child);
                } else {
                    Node* new_right = __put_inner(child->right, removed_id, &removed_row);
                    if (new_right == NULL) {
                        return NULL;
                    }
                    child->right = new_right;
                }
            }
        }
    }

    // New node was added, balance the tree again
    __update_node_height(node);
    return __rebalance_subtree(node);
}

// Insert or update a row by ID. Pointers in `row` are copied and do not need to be retained.
void TTree_put(TTree* tree, ID id, const Row* row) {
    // Create root node if tree is empty
    if (tree->root == NULL) {
        tree->root = __create_node(id, row);
    } else {
        Node* new_root = __put_inner(tree->root, id, row);
        if (new_root != NULL) {
            tree->root = new_root;
        }
    }
}

// Remove a row by ID. Returns whether a value was deleted.
bool TTree_remove(TTree* tree, ID id) {
    Node* node;
    size_t pos;
    bool exists = __get_bounding(tree, id, &node, &pos);
    if (!exists) {
        return false;
    }

    // TODO
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
        .go_right_next = true,
    };
}

// Advance the iterator to the next ID and Row.
// Returns whether there was a next element.
bool TTree_iter_next(TTreeIter* iter, ID* out_id, Row** out_row) {
    if (iter->nodes.length == 0) {
        return false;
    }

    Node* prev = iter->nodes.items[0];
    printf("root");
    for (int i = 1; i < iter->nodes.length; i++) {
        if (prev->left == iter->nodes.items[i]) {
            printf(" -> L");
        }
        if (prev->right == iter->nodes.items[i]) {
            printf(" -> R");
        }
        prev = iter->nodes.items[i];
    }
    Node* node = NodeList_get(&iter->nodes, iter->nodes.length - 1);
    printf(" | %d, %d/%d, go right %b\t ", iter->nodes.length, iter->pos, node->length,
           iter->go_right_next);
    *out_id = node->ids[iter->pos];
    *out_row = &node->data[iter->pos];
    if (++iter->pos < node->length) {
        return true;
    }

    iter->pos = 0;
    if (iter->go_right_next) {
        // Start iterating the right subtree
        iter->go_right_next = false;
        if (node->right != NULL) {
            node = node->right;
            while (node != NULL) {
                NodeList_append(&iter->nodes, node);
                node = node->left;
            }
            return true;
        }
    }

    while (true) {
        Node* prev = NodeList_pop(&iter->nodes);
        // We iterated all the nodes
        if (iter->nodes.length == 0) {
            break;
        }
        Node* current = NodeList_get(&iter->nodes, iter->nodes.length - 1);
        // We came up from the left, do the center and then the right next
        if (current->left == prev) {
            break;
        }
    }
    iter->go_right_next = true;
    return true;
}
