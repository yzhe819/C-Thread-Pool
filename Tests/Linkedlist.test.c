#include "../DataStructures/LinkedList.h"
#include <stdio.h>

int main() {
    struct LinkedList list = linked_list_constructor();

    for (int i = 0; i < 10; i++) {
        int* x = (int*)malloc(sizeof(int));
        *x = i;
        list.insert(&list, i, x);
    }

    list.remove(&list, 3);
    list.remove(&list, 7);

    for (int i = 0; i < 8; i++) {
        printf("%d\n", *(int*)list.retrieve(&list, i));
    }
}