gcc -O3 exprtest.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o exprtest -pthread;
gcc -O3 threadtest.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o threadtest -pthread;
gcc -O3 test.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o test -pthread;
