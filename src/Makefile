exprtest: exprtest.c
	gcc -O3 exprtest.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o exprtest

test: test.c
	gcc -O3 test.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o test

debug: test.c
	gcc -g test.c nvsim.c nvp.c rbtree.c nvht.c util.c nvlogger.c nvtxn.c allocpool.c -o test
