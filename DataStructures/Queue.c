#include "Queue.h"

void push(void* data, struct Queue* queue) {
    queue->list.insert(queue->list.length, data, &queue->list);
}

void* pop(struct Queue* queue) {
    void* data = queue->list.retrieve(0, &queue->list);
    queue->list.remove(0, &queue->list);
    return data;
}

struct Queue queue_constructor() {
    struct Queue queue;
    queue.list = linked_list_constructor();

    queue.push = push;
    queue.pop = pop;

    return queue;
}