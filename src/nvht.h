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

#define MAX_CHAIN_LENGTH 64

#define NVHT_HEADER_SIZE 4096
#define INIT_CAPACITY 32768

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
	struct nvp_t elem_nvp;
	int log_nvid;
	/* runtime data */
	struct nvht_element *elem_ptr;
	struct nvl_header *log_ptr;
	/* not used now. In future, it can be used as a overflow array */
	char data[0];
};

/*
 * nvid here is used for nvht header
 */
struct nvht_header *nvht_init(int nvid);
void nvht_put(struct nvht_header *h, char *kstr, int ksize, char *vstr, int vsize);
int nvht_get(struct nvht_header *h, char *k_str, int ksize, char *retvalue);
int nvht_remove(struct nvht_header *h, char *k_str, int ksize);
void nvht_free(struct nvht_header *h);
void print_nvht_image(struct nvht_header *h);

static void _nvht_rehash_move(struct nvht_header *h, struct nvp_t k, struct nvp_t v);
int nvht_rehash(struct nvht_header *h); /* non-static for test */

#endif

