#ifndef __NVHT_H__
#define __NVHT_H__
#include "nvp.h"

/*
 * nvht
 * hashtable for NVM
 * key is char array
 */

#define MAP_MISSING -2  /* No such element */
#define MAP_FULL -1 	/* nvht is full */
#define MAP_OK 0 	/* OK */

#define MAX_CHAIN_LENGTH 128 /* set to be cacheline friendly */

#define NVHT_HEADER_SIZE 4096
#define INIT_CAPACITY 4096

struct nvht_element {
	struct nvp_t key;
	struct nvp_t value;
	int use;
};

struct nvht_header {
	/* persistent data */
	int capacity;
	int size;
	int head_nvid;
	int log_nvid;
	struct nvp_t elems_nvp;
	/* runtime data */
	struct nvl_header *log_ptr;
	struct nvht_element *elems_ptr;
	/* not used now. In future, it can be used as a overflow array */
	char data[0];
};

typedef struct nvht_header NVHT;
/*
 * nvid here is used for nvht header
 */
NVHT *nvht_init(int nvid);
void nvht_put(NVHT *h, char *kstr, int ksize, char *vstr, int vsize);
/*
 * return -1 for fail, other for retvalue size
 */
int nvht_get(NVHT *h, char *kstr, int ksize, char *retvalue);
int nvht_remove(NVHT *h, char *kstr, int ksize);
void nvht_free(NVHT *h);
void print_nvht_image(NVHT *h);

static void _nvht_rehash_move(NVHT *h, struct nvp_t k, struct nvp_t v);
int nvht_rehash(NVHT *h); /* non-static for test */

#endif

