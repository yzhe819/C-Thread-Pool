#ifndef LinkedList_h
#define LinkedList_h

#include <stdio.h>
#include <stdlib.h>
#include "Node.h"

struct LinkedList {
    struct Node* head;
    int length;

    void (*insert)(int index, void* data, struct LinkedList* linked_list);
    void (*remove)(int index, struct LinkedList* linked_list);
    void* (*retrieve)(int index, struct LinkedList* linked_list);
};

struct LinkedList linked_list_constructor();

#endif /* LinkedList_h */