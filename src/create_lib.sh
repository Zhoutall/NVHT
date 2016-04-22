#/bin/bash
gcc -fPIC -c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c
ar rcs libnvht.a nvsim.o nvp.o rbtree.o nvht.o util.o nvlogger.o nvtxn.o
