#ifndef __ALLOCPOOL_H__
#define __ALLOCPOOL_H__

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))

/*
 * alloc pool: buddy system allocator algorithm for memory region
 * this structure can be persist to nvm
 */

struct pool_t {
	int size;
	int longest[0];
};

/*
 * size: chunk number, must be power of two
 */
struct pool_t *pool_init(int size);
void pool_remove(struct pool_t *p);

/*
 * arguments:
 *     p: pool structure
 *     size: alloc size
 */
/* NOT USE: code inline */
int pool_alloc(struct pool_t *p, int size);
/* NOT USE: code inline */
void pool_free(struct pool_t *p, int offset);

int pool_buddysize(struct pool_t *p, int offset);

unsigned fixsize(unsigned size);
#endif
