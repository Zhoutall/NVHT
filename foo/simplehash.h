#ifndef _SIMPLE_HASH
#define _SIMPLE_HASH

#include <stdio.h>
#include <stdlib.h>

#define INTISIZE 8
#define LOADFACTOR 0.75

// node flag
#define EMPTY 0
#define OK 1
#define DEL 2

// ATTENTION: user cannot use -1 (as return code)

struct node {
    int k, v;
    int flag;
};

struct simplehash {
    int count;
    int capacity;
    struct node *table;
};

static int hashFunc(int k) {
    return k; // k%7
}

struct simplehash *initSimplehash() {
    struct simplehash *sh = (struct simplehash *)malloc(sizeof(struct simplehash));
    sh->table = (struct node *)calloc(INTISIZE, sizeof(struct node));
    sh->count = 0;
    sh-> capacity = INTISIZE;
    return sh;
}

static void resize(struct simplehash *sh) {
    printf("resize\n");
    int newCapacity = sh->capacity*2;
    struct node *newTable = (struct node *)calloc(newCapacity, sizeof(struct node));
    struct node *tb = sh->table;
    for (int i = 0; i < sh->capacity; ++i) {
        if (tb[i].flag == OK)
        {
            int k = tb[i].k;
            int v = tb[i].v;
            int hash = hashFunc(k) % newCapacity;
            while(true) {
                if (newTable[hash].flag == EMPTY) {
                    newTable[hash].k = k;
                    newTable[hash].v = v;
                    newTable[hash].flag = OK;
                    break;
                } else {
                    hash++;
                }
            }
        }
    }
    sh->capacity = newCapacity;
    sh->table = newTable;
}

void put(struct simplehash *sh, int k, int v) {
    // printf("put %d\n", k);
    int hash = hashFunc(k) % (sh->capacity);
    struct node *tb = sh->table;
    while(true) {
        if (tb[hash].flag == EMPTY || tb[hash].flag == DEL) {
            tb[hash].k = k;
            tb[hash].v = v;
            tb[hash].flag = OK;
            sh->count++;
            // resize
            if (sh->count > (LOADFACTOR * sh->capacity)) {
                resize(sh);
            }
            break;
        } else {
            // flag is OK
            if (tb[hash].k == k) {
                tb[hash].v = v;
                break;
            }
            hash++;
        }
    }
}

static int getIndex(struct simplehash *sh, int k) {
    int hash = hashFunc(k) % (sh->capacity);
    struct node *tb = sh->table;
    while (true) {
        if (tb[hash].k == k && tb[hash].flag == OK) {
            return hash;
        } else if (tb[hash].flag == EMPTY) {
            return -1;
        }
        hash++;
    }
}

int get(struct simplehash *sh, int k) {
    // printf("get %d\n", k);
    int index = getIndex(sh, k);
    if (index != -1) {
        return sh->table[index].v;
    } else {
        return -1;
    }
}

void del(struct simplehash *sh, int k) {
    // printf("del %d\n", k);
    int index = getIndex(sh, k);
    if (index != -1) {
        sh->table[index].flag = DEL;
        sh->count--;
    }
}

void printTable(struct simplehash *sh) {
    struct node *tb = sh->table;
    printf("--hashtable--\n");
    for (int i = 0; i < sh->capacity; ++i) {
        if (tb[i].flag == OK) {
            printf("index %d: %d -> %d\n", i, tb[i].k, tb[i].v);
        }
    }
}

#endif
