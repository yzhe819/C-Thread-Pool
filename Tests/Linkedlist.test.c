#include "../DataStructures/LinkedList.h"
#include <stdio.h>

int main() {
    struct LinkedList list = linked_list_constructor();

    for (int i = 0; i < 10; i++) {
        int* x = (int*)malloc(sizeof(int));
        *x = i;
        list.insert(i, x, &list);
    }

    list.remove(3, &list);
    list.remove(7, &list);

    for (int i = 0; i < 8; i++) {
        printf("%d\n", *(int*)list.retrieve(i, &list));
    }
}