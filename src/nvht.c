#include <string.h>
#include <stdio.h>
#include "nvht.h"
#include "nvp.h"
#include "util.h"

static inline int nvht_elem_size(int capacity) {
	return sizeof(struct nvht_element) * capacity;
}

/*
 * Init a new nvht
 *
 * implementation tips:
 * 		one nvid for header
 * 		one nvid (randomly generated) for elements
 *
 * this function should attach these two nv region
 */
struct nvp_t nvht_init(int nvid) {
	if (nv_exist(nvid) != -1) {
		// map the exist nvht
		struct nvpitem *item = nvpcache_search_foritem(nvid);
		if (item != NULL) {
			return item->nvp;
		}
		struct nvp_t ret;
		ret.nvid = nvid;
		ret.nvoffset = 0; // no offset for nvht memory
		ret.size = NVHT_HEADER_SIZE;

		struct nvht_header *h = get_nvp(&ret);
		void *elem_addr = get_nvp(&h->elem_nvp);

		return ret;
	}
	// create a new one
	struct nvp_t nvht_header_nvp = alloc_nvp(nvid, NVHT_HEADER_SIZE);

	int elem_size = nvht_elem_size(INIT_CAPACITY);
	int elem_nvid = random_nvid();
//	printf("elem_nvid: %d\n", elem_nvid);
	struct nvp_t nvht_elem_nvp = alloc_nvp(elem_nvid, elem_size);

	void *header_addr = get_nvp(&nvht_header_nvp);
	void *elem_addr = get_nvp(&nvht_elem_nvp);
	memset(header_addr, 0, NVHT_HEADER_SIZE);
	memset(elem_addr, 0, elem_size);

	struct nvht_header *header = (struct nvht_header *)header_addr;
	header->capacity = INIT_CAPACITY;
	header->size = 0;
	header->head_nvid = nvid;
	header->elem_nvp.nvid = elem_nvid;
	header->elem_nvp.nvoffset = 0;
	header->elem_nvp.size = elem_size;
	return nvht_header_nvp;
}

/*
 * get the hash index for k
 */
static int nvht_hashindex(struct nvp_t nvht_p, char *k_str, int ksize) {
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);
	if (h->size >= (h->capacity / 2))
		return MAP_FULL;
	/* Find the best index */
	int curr = hash_string(k_str, ksize) % (h->capacity);
//	printf("%s curr %d\n", __func__, curr);
	/* Linear probing */
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		if (e[curr].use == 0)
			return curr;
		if (e[curr].use == 1) {
			struct nvp_t curr_k = e[curr].key;
			char *curr_k_str = nvalloc_getnvp(&curr_k);
			if (curr_k.size == ksize
					&& (strncmp(k_str, curr_k_str, ksize) == 0)) {
				return curr;
			}
		}
		curr = (curr + 1) % (h->capacity);
//		printf("%s probing curr %d\n", __func__, curr);
	}
	return MAP_FULL;
}

static int nvht_rehash(struct nvp_t nvht_p) {
//	printf("%s START REHASH\n", __func__);
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);

	struct nvp_t tmp = h->elem_nvp;

	int old_capacity = h->capacity;
	int new_capacity = old_capacity * 2;
	// alloc mem
	int elem_size = nvht_elem_size(new_capacity);
	int elem_nvid = random_nvid();
//	printf("elem_nvid: %d\n", elem_nvid);
	struct nvp_t new_e_nvp = alloc_nvp(elem_nvid, elem_size);
	void *new_elem_addr = get_nvp(&new_e_nvp);
	memset(new_elem_addr, 0, elem_size);
	h->capacity = new_capacity;
	h->size = 0;
	h->elem_nvp.nvid = elem_nvid;
	h->elem_nvp.nvoffset = 0;
	h->elem_nvp.size = elem_size;

	// do the rehash one by one
	int i;
	for (i=0; i<old_capacity; ++i) {
		if (e[i].use == 0)
			continue;
		nvht_put(nvht_p, e[i].key, e[i].value);
	}
	free_nvp(&tmp);
//	printf("%s END REHASH\n", __func__);
	return MAP_OK;
}

void nvht_put(struct nvp_t nvht_p, struct nvp_t k, struct nvp_t v) {
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);
	char *k_str = nvalloc_getnvp(&k);
	int index = nvht_hashindex(nvht_p, k_str, k.size);
	while (index == MAP_FULL) {
		nvht_rehash(nvht_p);
		index = nvht_hashindex(nvht_p, k_str, k.size);
	}
//	printf("Put k %s in index %d\n", k_str, index);
	// fix bug: e should be get by get_nvp again
	e = get_nvp(&h->elem_nvp);
	e[index].key = k;
	e[index].value = v;
	e[index].use = 1;
	h->size += 1;
	return;
}

struct nvp_t *nvht_get(struct nvp_t nvht_p, char *k_str, int ksize) {
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);
	int index = hash_string(k_str, ksize) % (h->capacity);
//	printf("%s, index %d\n", __func__, index);
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		int use = e[index].use;
		if (use == 1) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (strncmp(k_str, curr_k_str, ksize) == 0) {
				return &e[index].value;
			}
		}
		index = (index + 1) % (h->capacity);
//		printf("%s, probing index %d\n", __func__, index);
	}
	return NULL;
}

int nvht_remove(struct nvp_t nvht_p, char *k_str, int ksize) {
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);
	int index = nvht_hashindex(nvht_p, k_str, ksize);
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		int use = e[index].use;
		if (use == 1) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (strncmp(k_str, curr_k_str, ksize) == 0) {
				e[index].use = 0;
				h->size -= 1;
				return MAP_OK;
			}
		}
		index = (index + 1) % (h->capacity);
	}
	return MAP_MISSING;
}

void nvht_free(struct nvp_t nvht_p) {
	struct nvht_header *h = get_nvp(&nvht_p);
//	printf("%s elem_nvid %d\n", __func__, h->elem_nvp.nvid);
	free_nvp(&h->elem_nvp);
	free_nvp(&nvht_p);
}

int nvht_size(struct nvp_t nvht_p) {
	struct nvht_header *h = get_nvp(&nvht_p);
	return h->size;
}

void print_nvht_image(struct nvp_t nvht_p) {
	struct nvht_header *h = get_nvp(&nvht_p);
	struct nvht_element *e = get_nvp(&h->elem_nvp);
	int i;
	printf("~~~~~\n");
	for (i = 0; i < h->capacity; ++i) {
		printf("index %d: ", i);
		if (e[i].use == 0) {
			printf("[ ]\n");
		} else {
			printf("key %s\n", (char *) nvalloc_getnvp(&e[i].key));
		}
	}
	printf("~~~~~\n");
}
