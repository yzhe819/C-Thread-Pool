#include "LinkedList.h"

struct Node* create_Node(void* data);
void destroy_Node(struct Node* node_to_destroy);

struct Node* iterate(struct LinkedList* linked_list, int index);
void insert_Node(struct LinkedList* linked_list, int index, void* data);
void remove_Node(struct LinkedList* linked_list, int index);
void* retrieve_data(struct LinkedList* linked_list, int index);

struct LinkedList linked_list_constructor() {
    struct LinkedList new_linked_list;
    new_linked_list.head = NULL;
    new_linked_list.length = 0;

    new_linked_list.insert = insert_Node;
    new_linked_list.remove = remove_Node;
    new_linked_list.retrieve = retrieve_data;

    return new_linked_list;
}

struct Node* create_Node(void* data) {
    struct Node* new_node_address;
    // Allocate memory for the new node
    new_node_address = (struct Node*)malloc(sizeof(struct Node));
    struct Node new_node_instance;
    new_node_instance.data = data;
    new_node_instance.next = NULL;
    *new_node_address = new_node_instance;
    return new_node_address;
};

void destroy_Node(struct Node* node_to_destroy) {
    free(node_to_destroy->data);
    free(node_to_destroy);
};

struct Node* iterate(struct LinkedList* linked_list, int index) {
    if (index < 0 || index >= linked_list->length) {
        printf("Index out of bound...\n");
        exit(9);
    }
    struct Node* cursor = linked_list->head;
    for (int i = 0; i < index; i++) {
        cursor = cursor->next;
    }
    return cursor;
};

void insert_Node(struct LinkedList* linked_list, int index, void* data) {
    struct Node* node_to_insert = create_Node(data);
    if (index == 0) {
        node_to_insert->next = linked_list->head;
        linked_list->head = node_to_insert;
    } else {
        struct Node* cursor = iterate(linked_list, index - 1);
        node_to_insert->next = cursor->next;
        cursor->next = node_to_insert;
    }
    linked_list->length += 1;
}

void remove_Node(struct LinkedList* linked_list, int index) {
    if (index == 0) {
        struct Node* node_to_remove = linked_list->head;
        linked_list->head = node_to_remove->next;
        destroy_Node(node_to_remove);
    } else {
        struct Node* cursor = iterate(linked_list, index - 1);
        struct Node* node_to_remove = cursor->next;
        cursor->next = node_to_remove->next;
        destroy_Node(node_to_remove);
    }
    linked_list->length -= 1;
}

void* retrieve_data(struct LinkedList* linked_list, int index) {
    struct Node* cursor = iterate(linked_list, index);
    return cursor->data;
}