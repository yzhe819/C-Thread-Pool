#ifndef LinkedList_h
#define LinkedList_h

#include <stdio.h>
#include <stdlib.h>
#include "Node.h"

struct LinkedList {
    struct Node* head;
    int length;

    void (*insert)(struct LinkedList* linked_list, int index, void* data);
    void (*remove)(struct LinkedList* linked_list, int index);
    void* (*retrieve)(struct LinkedList* linked_list, int index);
};

struct LinkedList linked_list_constructor();

#endif /* LinkedList_h */