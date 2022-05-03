#include <stdio.h>
#include "LinkedList.h"

int main() {
    struct LinkedList_int list = linked_list_int_constructor();

    for (int i = 0; i < 10; i++) {
        list.insert(i, i, &list);
    }

    list.remove(3, &list);
    list.remove(7, &list);
    list.insert(1, 99, &list);

    for (int i = 0; i < 9; i++) {
        printf("%d\n", list.retrieve(i, &list));
    }

    list.retrieve(100, &list);  // error 9
}