#ifndef Queue_h
#define Queue_h

#include "LinkedList.h"

struct Queue {
    struct LinkedList list;

    void (*push)(struct Queue* queue, void* data);
    void* (*pop)(struct Queue* queue);
};

struct Queue queue_constructor(void);

#endif /* Queue_h */