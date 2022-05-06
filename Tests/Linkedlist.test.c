#include "../DataStructures/LinkedList.h"
#include <stdio.h>

int main() {
    struct LinkedList list = linked_list_constructor();

    for (int i = 0; i < 10; i++) {
        int a = i;
        list.insert(&list, i, &a, Int, 1);
    }

    list.remove(&list, 3);
    list.remove(&list, 7);

    for (int i = 0; i < 8; i++) {
        printf("%d\n", *(int*)list.retrieve(&list, i));
    }
}