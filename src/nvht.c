#include <string.h>
#include <stdio.h>
#include "nvht.h"
#include "nvp.h"
#include "util.h"
#include "nvlogger.h"
#include "nvtxn.h"

static inline int nvht_elem_size(int capacity) {
	return sizeof(struct nvht_element) * capacity;
}

static struct nvp_t gen_nvht_nvp(int nvid) {
	struct nvp_t ret;
	ret.nvid = nvid;
	ret.nvoffset = 0; // no offset for nvht memory
	ret.size = NVHT_HEADER_SIZE;
	return ret;
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
NVHT *nvht_init(int nvid) {
	if (nv_exist(nvid) != -1) {
		// map the exist nvht
		struct nvpitem *item = nvpcache_search_foritem(nvid);
		if (item != NULL) {
			return item->vaddr;
		}
		struct nvp_t nvht_nvp = gen_nvht_nvp(nvid);
		NVHT *h = get_nvp(&nvht_nvp);
		h->log_ptr = nvl_get(h->log_nvid, 0);
		nvtxn_recover(h->log_ptr);
		/* init nvht_header runtime data (elem_ptr must init after recover) */
		h->elems_ptr = get_nvp(&h->elems_nvp);
		return h;
	}
	// create a new one
	struct nvp_t nvht_header_nvp = alloc_nvp(nvid, NVHT_HEADER_SIZE);

	int elem_size = nvht_elem_size(INIT_CAPACITY);
	int elem_nvid = random_nvid();
	struct nvp_t nvht_elem_nvp = alloc_nvp(elem_nvid, elem_size);

	void *header_addr = get_nvp(&nvht_header_nvp);
	void *elem_addr = get_nvp(&nvht_elem_nvp);
	memset(header_addr, 0, NVHT_HEADER_SIZE);
	memset(elem_addr, 0, elem_size);
	// init log region
	int log_nvid = random_nvid();
	struct nvl_header *nvlh = nvl_get(log_nvid, 0);

	NVHT *header = (NVHT *)header_addr;
	header->capacity = INIT_CAPACITY;
	header->size = 0;
	header->head_nvid = nvid;
	header->elems_nvp.nvid = elem_nvid;
	header->elems_nvp.nvoffset = 0;
	header->elems_nvp.size = elem_size;
	header->log_nvid = log_nvid;
	header->elems_ptr = (struct nvht_element *)elem_addr;
	header->log_ptr = nvlh;
	return header;
}

/*
 * get the hash index for k
 */
static int nvht_hashindex(NVHT *h, char *k_str, int ksize) {
	struct nvht_element *e = h->elems_ptr;
//	if (h->size >= (h->capacity / 2))
	if (h->size >= h->capacity)
		return MAP_FULL;
	/* Find the best index */
	int curr = hash_string(k_str, ksize) % (h->capacity);
	/* Linear probing */
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		if (e[curr].use == 0)
			return curr;
		if (e[curr].use == 1) {
			struct nvp_t curr_k = e[curr].key;
			char *curr_k_str = nvalloc_getnvp(&curr_k);
			if (curr_k.size == ksize
					&& (memcmp(k_str, curr_k_str, ksize) == 0)) {
				return curr;
			}
		}
		curr = (curr + 1) % (h->capacity);
	}
	return MAP_FULL;
}

int nvht_rehash(NVHT *h) {
	printf("%s rehash load %d %d %f\n", __func__, h->size, h->capacity, (double)h->size/h->capacity);
	struct nvht_element *e = h->elems_ptr;

	struct nvtxn_info txn = nvtxn_start(h->log_ptr);
	struct nvp_t old_elem_nvp = h->elems_nvp;

	int old_capacity = h->capacity;
	int new_capacity = old_capacity * 2;
	// alloc mem
	int elem_size = nvht_elem_size(new_capacity);
	int elem_nvid = random_nvid();

	nvtxn_record_nv_update(&txn, NV_ALLOC, elem_nvid);
	struct nvp_t new_e_nvp = alloc_nvp(elem_nvid, elem_size);
	void *new_elem_addr = get_nvp(&new_e_nvp);
	memset(new_elem_addr, 0, elem_size);

	nvtxn_record_data_update(&txn, NVHT_HEADER, gen_nvht_nvp(h->head_nvid), 0, h,
				sizeof(NVHT));
	h->capacity = new_capacity;
	h->size = 0;
	h->elems_nvp.nvid = elem_nvid;
	h->elems_nvp.nvoffset = 0;
	h->elems_nvp.size = elem_size;
	h->elems_ptr = new_elem_addr;

	// do the rehash one by one
	int i;
	for (i=0; i<old_capacity; ++i) {
		if (e[i].use == 0)
			continue;
		_nvht_rehash_move(h, e[i].key, e[i].value);
	}
	nvtxn_commit(&txn);
	free_nvp(&old_elem_nvp);
	return MAP_OK;
}

static void _nvht_rehash_move(NVHT *h, struct nvp_t k, struct nvp_t v) {
	char *k_str = nvalloc_getnvp(&k);
	int index = nvht_hashindex(h, k_str, k.size);
	while (index == MAP_FULL) {
		printf("Bad hash function.\n");
		nvht_rehash(h);
		index = nvht_hashindex(h, k_str, k.size);
	}
	struct nvht_element *e = h->elems_ptr;
	e[index].key = k;
	e[index].value = v;
	e[index].use = 1;
	h->size += 1;
	return;
}

void nvht_put(NVHT *h, char *kstr, int ksize, char *vstr, int vsize) {
	int index = nvht_hashindex(h, kstr, ksize);
	while (index == MAP_FULL) {
		nvht_rehash(h);
		index = nvht_hashindex(h, kstr, ksize);
	}
	struct nvtxn_info txn = nvtxn_start(nvl_get(h->log_nvid, 0));
	struct nvp_t v = txn_make_nvp_withdata(&txn, vstr, vsize);
	struct nvht_element *e = h->elems_ptr;
	// free old value if key exists (do a replace)
	if (e[index].use == 1) {
		struct nvp_t oldv_nvp = e[index].value;
		txn_nvalloc_free(&txn, &oldv_nvp);
		nvtxn_record_data_update(&txn, NVHT_PUT, h->elems_nvp,
				index * sizeof(struct nvht_element), e + index,
				sizeof(struct nvht_element));
		e[index].value = v;
	} else {
		struct nvp_t k = txn_make_nvp_withdata(&txn, kstr, ksize);
		nvtxn_record_data_update(&txn, NVHT_PUT, h->elems_nvp,
				index * sizeof(struct nvht_element), e + index,
				sizeof(struct nvht_element));
		e[index].key = k;
		e[index].value = v;
		e[index].use = 1;
//		nvtxn_record_data_update(&txn, NVHT_HEADER, gen_nvht_nvp(h->head_nvid), 0, h,
//				sizeof(NVHT));
		h->size += 1;
	}
	nvtxn_commit(&txn);
	return;
}

int nvht_get(NVHT *h, char *kstr, int ksize, char *retvalue) {
	if (retvalue == NULL)
		return -1;
	struct nvht_element *e = h->elems_ptr;
	int index = hash_string(kstr, ksize) % (h->capacity);
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		int use = e[index].use;
		if (use == 1 && ksize == e[index].key.size) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (memcmp(kstr, curr_k_str, ksize) == 0) {
				memcpy(retvalue, nvalloc_getnvp(&e[index].value), e[index].value.size);
				return e[index].value.size;
			}
		} else if (use == 0) {
			break;
		}
		index = (index + 1) % (h->capacity);
	}
	return -1;
}

int nvht_remove(NVHT *h, char *kstr, int ksize) {
	struct nvht_element *e = h->elems_ptr;
	int index = nvht_hashindex(h, kstr, ksize);
	int i;
	for (i = 0; i < MAX_CHAIN_LENGTH; ++i) {
		int use = e[index].use;
		if (use == 1) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (memcmp(kstr, curr_k_str, ksize) == 0) {
				struct nvtxn_info txn = nvtxn_start(nvl_get(h->log_nvid, 0));
				nvtxn_record_data_update(&txn, NVHT_REMOVE, h->elems_nvp,
						index * sizeof(struct nvht_element), e + index, sizeof(struct nvht_element));
				e[index].use = 0;
				nvtxn_record_data_update(&txn, NVHT_HEADER, gen_nvht_nvp(h->head_nvid), 0, h, sizeof(NVHT));
				h->size -= 1;
				// free nvp
				txn_nvalloc_free(&txn, &e[index].key);
				txn_nvalloc_free(&txn, &e[index].value);
				nvtxn_commit(&txn);
				return MAP_OK;
			}
		}
		index = (index + 1) % (h->capacity);
	}
	return MAP_MISSING;
}

void nvht_free(NVHT *h) {
	free_nvp(&h->elems_nvp);
	nvl_free(h->log_nvid);
	struct nvp_t nvht_nvp = gen_nvht_nvp(h->head_nvid);
	free_nvp(&nvht_nvp);
}

void print_nvht_image(NVHT *h) {
	struct nvht_element *e = h->elems_ptr;
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
