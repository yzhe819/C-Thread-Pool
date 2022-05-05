#include "../DataStructures/Queue.h"
#include <stdio.h>

int main() {
    struct Queue list = queue_constructor();

    for (int i = 0; i < 10; i++) {
        int* x = (int*)malloc(sizeof(int));
        *x = i;
        list.push(&list, x);
    }

    for (int i = 0; i < 10; i++) {
        int a = *(int*)list.pop(&list);
        printf("%d\n", a);
    }
}