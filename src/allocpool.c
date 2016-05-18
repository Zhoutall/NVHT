#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "allocpool.h"
#include "nvtxn.h"

unsigned fixsize(unsigned size) {
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;
	return size + 1;
}

/* for test init, use nvalloc_init in the program */
struct pool_t *pool_init(int size) {
	if (size < 1) {
		return NULL;
	}
	if (!IS_POWER_OF_2(size)) {
		size = fixsize(size);
	}
	struct pool_t *pool;
	pool = (struct pool_t *) malloc(2 * size * sizeof(int));
	pool->size = size;
	int nodesize = size * 2;
	int i;
	/* set tree node size */
	for (i = 0; i < 2 * size - 1; ++i) {
		if (IS_POWER_OF_2(i + 1)) {
			nodesize /= 2;
		}
		pool->longest[i] = nodesize;
	}
	return pool;
}

void pool_remove(struct pool_t *p) {
	free(p);
}

int pool_alloc(struct pool_t *p, int chunk_num) {
	int index = 0;

	if (p == NULL)
		return -1;

	if (chunk_num < 1) {
		chunk_num = 1;
	} else if (!IS_POWER_OF_2(chunk_num)) {
		chunk_num = fixsize(chunk_num);
	}

	if (p->longest[index] < chunk_num)
		return -1;

	/* find the match node */
	int nodesize;
	for (nodesize = p->size; nodesize != chunk_num; nodesize /= 2) {
		if (p->longest[LEFT_LEAF(index)] >= chunk_num)
			index = LEFT_LEAF(index);
		else
			index = RIGHT_LEAF(index);
	}

	/* mark use */
	p->longest[index] = 0;
	int offset = (index + 1) * nodesize - p->size;

	/* record use to parents */
	while (index) {
		index = PARENT(index);
		p->longest[index] = MAX(p->longest[LEFT_LEAF(index)],
				p->longest[RIGHT_LEAF(index)]);
	}

	return offset;
}

void pool_free(struct pool_t *p, int offset) {
	if (p == NULL || offset < 0 || offset >= p->size) {
		return;
	}

	int nodesize = 1;
	/* index in leaf node */
	int index = offset + p->size - 1;

	/* find the node through the path to root */
	for (; p->longest[index]; index = PARENT(index)) {
		nodesize *= 2;
		if (index == 0)
			return;
	}

	p->longest[index] = nodesize;

	/* udpate parent use infomation */
	int left_longest, right_longest;
	while (index) {
		index = PARENT(index);
		nodesize *= 2;

		left_longest = p->longest[LEFT_LEAF(index)];
		right_longest = p->longest[RIGHT_LEAF(index)];

		if (left_longest + right_longest == nodesize)
			p->longest[index] = nodesize;
		else
			p->longest[index] = MAX(left_longest, right_longest);
	}
}

int pool_buddysize(struct pool_t *p, int offset) {
	if (p == NULL || offset < 0 || offset >= p->size) {
		return 0;
	}

	int nodesize = 1, index = offset + p->size - 1;
	for (; p->longest[index]; index = PARENT(index))
		nodesize *= 2;

	return nodesize;
}
