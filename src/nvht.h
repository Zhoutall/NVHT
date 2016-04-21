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

#define MAX_CHAIN_LENGTH 16

#define NVHT_HEADER_SIZE 4096
#define INIT_CAPACITY 256

struct nvht_element {
	struct nvp_t key;
	struct nvp_t value;
	int use;
};

struct nvht_header {
	int capacity;
	int size;
	int head_nvid;
	struct nvp_t elem_nvp;
	int log_nvid;
	/* not used now. In future, it can be used as a overflow array */
	struct nvht_element data[0];
};

/*
 * nvid here is used for nvht header
 */
struct nvp_t nvht_init(int nvid);
void nvht_put(struct nvp_t nvht_p, char *kstr, int ksize, char *vstr, int vsize);
struct nvp_t *nvht_get(struct nvp_t nvht_p, char *k_str, int ksize);
int nvht_remove(struct nvp_t nvht_p, char *k_str, int ksize);
void nvht_free(struct nvp_t nvht_p);
int nvht_size(struct nvp_t nvht_p);
void print_nvht_image(struct nvp_t nvht_p);

static void _nvht_rehash_move(struct nvp_t nvht_p, struct nvp_t k, struct nvp_t v);

#endif

