#include "Queue.h"

void push(struct Queue* queue, void* data) {
    queue->list.insert(&queue->list, queue->list.length, data);
}

void* pop(struct Queue* queue) {
    void* data = queue->list.retrieve(&queue->list, 0);
    queue->list.remove(&queue->list, 0);
    return data;
}

struct Queue queue_constructor() {
    struct Queue queue;
    queue.list = linked_list_constructor();

    queue.push = push;
    queue.pop = pop;

    return queue;
}