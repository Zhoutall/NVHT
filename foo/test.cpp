#include "simplehash.h"

int main() {
    struct simplehash *sh = initSimplehash();
    put(sh, 9, 9);
    put(sh, 1, 1);
    put(sh, 3, 3);
    printTable(sh);
    put(sh, 2, 2);
    put(sh, 6, 6);
    put(sh, 7, 7);
    put(sh, 4, 4);
    put(sh, 5, 5);
    printTable(sh);
    put(sh, 9, 9);
    put(sh, 1, 10);
    put(sh, 1, 11);
    printf("%d\n", get(sh, 1));
    printf("%d\n", get(sh, 2));
    printf("%d\n", get(sh, 3));
    printf("%d\n", get(sh, 5));
    del(sh, 5);
    del(sh, 9);
    printf("%d\n", get(sh, 5));
    printf("%d\n", get(sh, 11));
    printTable(sh);
}