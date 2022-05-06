#include "../DataStructures/Queue.h"
#include <stdio.h>
int main() {
    struct Queue list = queue_constructor();

    // test 1:
    // for (int i = 0; i < 10; i++) {
    //     char x[3] = "xyz";
    //     list.push(&list, &x, Char, 3);
    // }

    // for (int i = 0; i < 10; i++) {
    //     printf("%c\n", ((char*)list.peek(&list))[2]);
    //     list.pop(&list);
    // }

    // test 2:
    for (int i = 0; i < 10; i++) {
        int x[4] = {i - 1, i, i + 1, i + 2};
        list.push(&list, &x, Int, 4);
    }

    for (int i = 0; i < 10; i++) {
        printf("[%d, %d, %d, %d]\n", ((int*)list.peek(&list))[0],
               ((int*)list.peek(&list))[1], ((int*)list.peek(&list))[2],
               ((int*)list.peek(&list))[3]);
        list.pop(&list);
    }

    queue_destructor(&list);
}