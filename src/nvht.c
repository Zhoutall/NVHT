#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "nvht.h"
#include "nvp.h"
#include "util.h"
#include "nvlogger.h"
#include "nvtxn.h"

pthread_spinlock_t m;

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
	pthread_spin_init(&m, PTHREAD_PROCESS_PRIVATE);
	if (nv_exist(nvid) != -1) {
		// map the exist nvht
		struct nvpitem *item = nvpcache_search_foritem(nvid);
		if (item != NULL) {
			return item->vaddr;
		}
		struct nvp_t nvht_nvp = gen_nvht_nvp(nvid);
		NVHT *h = get_nvp(&nvht_nvp);
		if (h->log_nvid != 0 && nv_exist(h->log_nvid)) {
			// not crash when init
			h->log_ptr = nvl_get(h->log_nvid, 0);
			nvtxn_recover(h->log_ptr);
			/* init nvht_header runtime data (elem_ptr must init after recover) */
			h->elems_ptr = get_nvp(&h->elems_nvp);
			return h;
		}
		// crash when init
		int elem_size = nvht_elem_size(INIT_CAPACITY);
		void *header_addr = get_nvp(&nvht_nvp);
		NVHT *header = (NVHT *)header_addr;
		header->capacity = INIT_CAPACITY;
		header->size = 0;
		header->head_nvid = nvid;
		struct nvp_t nvht_elem_nvp;
		if (header->elems_nvp.nvid == 0 || !nv_exist(header->elems_nvp.nvid)) {
			int elem_nvid = random_nvid();
			header->elems_nvp.nvid = elem_nvid;
			nvht_elem_nvp = alloc_nvp(elem_nvid, elem_size);
		} else {
			nvht_elem_nvp.nvid = header->elems_nvp.nvid;
			nvht_elem_nvp.nvoffset = 0;
			nvht_elem_nvp.size = elem_size;
		}
		void *elem_addr = get_nvp(&nvht_elem_nvp);
		memset(elem_addr, 0, elem_size);
		header->elems_nvp.nvoffset = 0;
		header->elems_nvp.size = elem_size;

		int log_nvid = random_nvid();
		header->log_nvid = log_nvid;
		struct nvl_header *nvlh = nvl_get(log_nvid, 0);

		header->elems_ptr = (struct nvht_element *)elem_addr;
		header->log_ptr = nvlh;

		return header;
	}
	// create a new one
	struct nvp_t nvht_header_nvp = alloc_nvp(nvid, NVHT_HEADER_SIZE);

	int elem_size = nvht_elem_size(INIT_CAPACITY);
	NVHT *header = (NVHT *)get_nvp(&nvht_header_nvp);
	memset((void *)header, 0, NVHT_HEADER_SIZE);
	header->capacity = INIT_CAPACITY;
	header->size = 0;
	header->head_nvid = nvid;

	int elem_nvid = random_nvid();
	header->elems_nvp.nvid = elem_nvid;
	struct nvp_t nvht_elem_nvp = alloc_nvp(elem_nvid, elem_size);
	void *elem_addr = get_nvp(&nvht_elem_nvp);
	memset(elem_addr, 0, elem_size);
	header->elems_nvp.nvoffset = 0;
	header->elems_nvp.size = elem_size;

	// init log region
	int log_nvid = random_nvid();
	header->log_nvid = log_nvid;
	struct nvl_header *nvlh = nvl_get(log_nvid, 0);

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
	if (h->size >= (h->capacity * MAX_LOAD_FACTOR))
		return MAP_FULL;
	/* Find the best index */
	int curr = hash_string(k_str, ksize) % (h->capacity);
	/* Linear probing */
	int i;
	for (i = 0; i < MAX_PROBING_LENGTH; ++i) {
		if (e[curr].status == 0 || e[curr].status == 2)
			return curr;
		if (e[curr].status == 1 && e[curr].key.size == ksize) {
			char *curr_k_str = nvalloc_getnvp(&e[curr].key);
			if (memcmp(k_str, curr_k_str, ksize) == 0) {
				return curr;
			}
		}
		curr = (curr + 1) % (h->capacity);
	}
	return MAP_FULL;
}

int nvht_rehash(NVHT *h) {
//	printf("Rehash load factor %d %f\n", h->capacity, (double)h->size/h->capacity);
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
		if (e[i].status == 0)
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
		printf("Bad hash function, do rehash again!!!!\n");
		nvht_rehash(h);
		index = nvht_hashindex(h, k_str, k.size);
	}
	struct nvht_element *e = h->elems_ptr;
	e[index].key = k;
	e[index].value = v;
	e[index].status = 1;
	h->size += 1;
	return;
}

void nvht_put(NVHT *h, char *kstr, int ksize, char *vstr, int vsize) {
	pthread_spin_lock(&m);
	int index = nvht_hashindex(h, kstr, ksize);
	while (index == MAP_FULL) {
		nvht_rehash(h);
		index = nvht_hashindex(h, kstr, ksize);
	}
	struct nvtxn_info txn = nvtxn_start(nvl_get(h->log_nvid, 0));
	struct nvp_t v = txn_make_nvp_withdata(&txn, vstr, vsize);
	struct nvht_element *e = h->elems_ptr;
	// free old value if key exists (do a replace)
	if (e[index].status == 1) {
		struct nvp_t oldv_nvp = e[index].value;
		nvalloc_free(&txn, &oldv_nvp);
		nvtxn_record_data_update(&txn, NVHT_PUT, h->elems_nvp,
				index * sizeof(struct nvht_element), e + index,
				sizeof(struct nvht_element));
		e[index].value = v;
	} else {
		struct nvp_t k = txn_make_nvp_withdata(&txn, kstr, ksize);
		nvtxn_record_data_update(&txn, NVHT_PUT_NEW, h->elems_nvp,
				index * sizeof(struct nvht_element), NULL,
				sizeof(struct nvht_element));
		e[index].key = k;
		e[index].value = v;
		e[index].status = 1;
		nvtxn_record_data_update(&txn, NVHT_HEADER, gen_nvht_nvp(h->head_nvid), 0, h,
				sizeof(NVHT));
		h->size += 1;
	}
	nvtxn_commit(&txn);
	pthread_spin_unlock(&m);
	return;
}

int nvht_get(NVHT *h, char *kstr, int ksize, char **retvalue) {
	pthread_spin_lock(&m);
	struct nvht_element *e = h->elems_ptr;
	int index = hash_string(kstr, ksize) % (h->capacity);
	int i;
	for (i = 0; i < MAX_PROBING_LENGTH; ++i) {
		int use = e[index].status;
		if (use == 0) break;
		if (use == 1 && ksize == e[index].key.size) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (memcmp(kstr, curr_k_str, ksize) == 0) {
				*retvalue = nvalloc_getnvp(&e[index].value);
				//memcpy(retvalue, nvalloc_getnvp(&e[index].value), e[index].value.size);
				pthread_spin_unlock(&m);
				return e[index].value.size;
			}
		}
		index = (index + 1) % (h->capacity);
	}
	pthread_spin_unlock(&m);
	return -1;
}

int nvht_remove(NVHT *h, char *kstr, int ksize) {
	pthread_spin_lock(&m);
	struct nvht_element *e = h->elems_ptr;
	int index = nvht_hashindex(h, kstr, ksize);
	int i;
	for (i = 0; i < MAX_PROBING_LENGTH; ++i) {
		int use = e[index].status;
		if (use == 0) break;
		if (use == 1 && e[index].key.size == ksize) {
			char *curr_k_str = nvalloc_getnvp(&e[index].key);
			if (memcmp(kstr, curr_k_str, ksize) == 0) {
				struct nvtxn_info txn = nvtxn_start(nvl_get(h->log_nvid, 0));
				nvtxn_record_data_update(&txn, NVHT_REMOVE, h->elems_nvp,
						index * sizeof(struct nvht_element), e + index, sizeof(struct nvht_element));
				e[index].status = 2;
				nvtxn_record_data_update(&txn, NVHT_HEADER, gen_nvht_nvp(h->head_nvid), 0, h, sizeof(NVHT));
				h->size -= 1;
				// free nvp
				nvalloc_free(&txn, &e[index].key);
				nvalloc_free(&txn, &e[index].value);
				nvtxn_commit(&txn);
				pthread_spin_unlock(&m);
				return MAP_OK;
			}
		}
		index = (index + 1) % (h->capacity);
	}
	pthread_spin_unlock(&m);
	return MAP_MISSING;
}

void nvht_free(NVHT *h) {
	pthread_spin_destroy(&m);
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
		if (e[i].status == 0) {
			printf("[ ]\n");
		} else {
			printf("key %s\n", (char *) nvalloc_getnvp(&e[i].key));
		}
	}
	printf("~~~~~\n");
}
