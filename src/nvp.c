#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "nvp.h"

#ifdef USE_HASHCACHE
struct nvpitem *base = NULL;

void *nvpcache_search(int _nvid) {
	struct nvpitem *item = NULL;
	HASH_FIND_INT(base, &_nvid, item);
	if (item == NULL) return NULL;
	return item->vaddr;
}

struct nvpitem *nvpcache_search_foritem(int _nvid) {
	struct nvpitem *item;
	HASH_FIND_INT(base, &_nvid, item);
	return item;
}

int nvpcache_insert(int _nvid, void *addr) {
	struct nvpitem *_nvpitem = malloc(sizeof(struct nvpitem));
	_nvpitem->nvid = _nvid;
	_nvpitem->vaddr = addr;
	HASH_ADD_INT(base, nvid, _nvpitem);
}

int nvpcache_delete(int _nvid) {
	struct nvpitem *item = nvpcache_search_foritem(_nvid);
	if (item == NULL) return -1;
	HASH_DEL(base, item);
	free(item);
	return 0;
}

#else
static struct rb_root nvpcache_root = RB_ROOT;

void *nvpcache_search(int _nvid) {
    struct nvpitem *item = nvpcache_search_rb(&nvpcache_root, _nvid);
    if (!item) return NULL;
    return item->vaddr;
}

struct nvpitem *nvpcache_search_foritem(int _nvid) {
   return nvpcache_search_rb(&nvpcache_root, _nvid);
}

static struct nvpitem *nvpcache_search_rb(struct rb_root *root, int _nvid) {
    struct rb_node *node = root->rb_node;
    while (node) {
        struct nvpitem *data = container_of(node, struct nvpitem, node);
        if (_nvid < data->nvid)
            node = node->rb_left;
        else if (_nvid > data->nvid)
            node = node->rb_right;
        else
            return data;
    }
    return NULL;
}

int nvpcache_insert(int _nvid, void *addr) {
	struct nvpitem *_nvpitem = malloc(sizeof(struct nvpitem));
	_nvpitem->nvid = _nvid;
	_nvpitem->vaddr = addr;
    return nvpcache_insert_rb(&nvpcache_root, _nvpitem);
}

static int nvpcache_insert_rb(struct rb_root *root, struct nvpitem *_nvpitem) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    int _nvid = _nvpitem->nvid;
    while (*new) {
        struct nvpitem *this = container_of(*new, struct nvpitem, node);
        parent = *new;
        if (_nvid < this->nvid)
            new = &((*new)->rb_left);
        else if (_nvid > this->nvid)
            new = &((*new)->rb_right);
        else
            return -1;
    }
    rb_link_node(&_nvpitem->node, parent, new);
    rb_insert_color(&_nvpitem->node, root);
    return 0;
}

int nvpcache_delete(int _nvid) {
    return nvpcache_delete_rb(&nvpcache_root, _nvid);
}

static int nvpcache_delete_rb(struct rb_root *root, int _nvid) {
    struct nvpitem *item = nvpcache_search_rb(root, _nvid);
    if (!item) return -1;
    rb_erase(&item->node, root);
    free(item);
    return 0;
}
#endif

struct nvp_t alloc_nvp(int _nvid, int size) {
	// TODO check nvid not exist
	void *vaddr = nv_get(_nvid, size);
	nvpcache_insert(_nvid, vaddr);
	struct nvp_t nvp_ret;
	nvp_ret.nvid = _nvid;
	nvp_ret.nvoffset = 0;
	nvp_ret.size = size;
	return nvp_ret;
}

void *get_nvp(struct nvp_t *nvp) {
	if (nvp == NULL) {
		printf("%s: NULL argument\n", __func__);
		return NULL;
	}
	// TODO check nvid exist in shm
	void *vaddr = nvpcache_search(nvp->nvid);
	if (vaddr != NULL) {
		return (char *)vaddr + nvp->nvoffset;
	}
	vaddr = nv_map(nvp->nvid);
	nvpcache_insert(nvp->nvid, vaddr);
	return (char *)vaddr + nvp->nvoffset;
}

void free_nvp(struct nvp_t *nvp) {
	// TODO check nvid exist in shm
	void *vaddr = nvpcache_search(nvp->nvid);
	if (vaddr != NULL) {
		nvpcache_delete(nvp->nvid);
	}
	nv_remove(nvp->nvid);
}

/*
 * nv_allocator
 * bitmap & object pool
 */

#define USEPOOL

static void *heap_base_addr = 0;
static int *wear_leveling_mgr = 0; // wear-leveling

#define HEAP_SIZE_MIN 131072 // 128kB
#define HEAP_CHUNK_SIZE 128 // 128B per chunk
#define HEAP_MAGIC 0x5A1AA1A5
/*
 * memory image:
 * HEAP_MAGIC | h_nvid | size | BITMAP or POOL | data
 */

/*
 * helper function
 */
static int get_heap_size() {
	assert(heap_base_addr != 0);
	int *hsize_ptr = heap_base_addr + 2 * sizeof(int);
	return *hsize_ptr;
}

static int get_heap_nvid() {
	assert(heap_base_addr != 0);
	int *hvid_ptr = heap_base_addr + sizeof(int);
	return *hvid_ptr;
}

#ifdef USEPOOL
static struct pool_t *get_pool_addr() {
	assert(heap_base_addr != 0);
	return heap_base_addr + 3 * sizeof(int);
}

static int get_pool_size() {
	assert(heap_base_addr != 0);
	struct pool_t *p = get_pool_addr();
	return 2 * p->size * sizeof(int);
}

static void nvalloc_init_core(int h_nvid, int heap_size, int realsize) {
	int *go_ptr = (int *) heap_base_addr;
	int *magic_ptr = go_ptr;
	++go_ptr;
	*go_ptr = h_nvid;
	++go_ptr;
	*go_ptr = heap_size;
	++go_ptr;
	// set pool
	struct pool_t *pool = (struct pool_t *) go_ptr;
	pool->size = realsize;
	int nodesize = realsize * 2;
	int i;
	/* set tree node size */
	int arrlen = 2 * realsize - 1;
	for (i = 0; i < arrlen; ++i) {
		if (IS_POWER_OF_2(i + 1)) {
			nodesize /= 2;
		}
		pool->longest[i] = nodesize;
	}
	*magic_ptr = HEAP_MAGIC;
}

static void wear_leveling_init(int heap_size) {
	int realsize = (heap_size - 3 * sizeof(int)) / (HEAP_CHUNK_SIZE + 2 * sizeof(int)); /* no need to fix */
	int arrlen = 2 * realsize - 1;
	/* init wear-leveling : init to 0 on each startup */
	wear_leveling_mgr = (int *)malloc(arrlen * sizeof(int));
	memset(wear_leveling_mgr, 0, arrlen * sizeof(int));
}

void nvalloc_init(int h_nvid, int heap_size) {
	if (nv_exist(h_nvid) != -1) {
		// nvpcache
		heap_base_addr = nvpcache_search(h_nvid);
		if (heap_base_addr == NULL) {
			heap_base_addr = nv_map(h_nvid);
			nvpcache_insert(h_nvid, heap_base_addr);
		}
		int *magic_ptr = (int *)heap_base_addr;
		if (*magic_ptr != HEAP_MAGIC) {
			printf("NV ALLOC HEAP MAGIC error!\n");
			// do a recovery
			if (heap_size < HEAP_SIZE_MIN) {
				heap_size = HEAP_SIZE_MIN;
			}
			int realsize = (heap_size - 3 * sizeof(int))
					/ (HEAP_CHUNK_SIZE + 2 * sizeof(int));
			if (!IS_POWER_OF_2(realsize)) {
				realsize = fixsize(realsize);
			}
			heap_size = 3 * sizeof(int)
					+ (2 * sizeof(int) + HEAP_CHUNK_SIZE) * realsize;
			nvalloc_init_core(h_nvid, heap_size, realsize);
		}
		wear_leveling_init(get_heap_size());
		return;
	}
	/*
	 * realsize: is power of 2
	 * int \ int \ int \ 2*realsize*int \ realsize*HEAP_CHUNK_SIZE
	 */
	if (heap_size < HEAP_SIZE_MIN) {
		heap_size = HEAP_SIZE_MIN;
	}
	/*
	 * fix heap_size
	 */
	int realsize = (heap_size - 3 * sizeof(int))
			/ (HEAP_CHUNK_SIZE + 2 * sizeof(int));
	if (!IS_POWER_OF_2(realsize)) {
		realsize = fixsize(realsize);
	}
	heap_size = 3 * sizeof(int)
			+ (2 * sizeof(int) + HEAP_CHUNK_SIZE) * realsize;
	heap_base_addr = nv_get(h_nvid, heap_size);
	nvalloc_init_core(h_nvid, heap_size, realsize);
	wear_leveling_init(get_heap_size());
	// nvpcache
	// do it at last because it does not affect consistency
	nvpcache_insert(h_nvid, heap_base_addr);
	return;
}

/*
 * helper function for pool meta-data log record
 */

/*
 * isalloc: 1 alloc 0 free
 */
void pool_tree_recovery(struct pool_txn_record_t *data) {
	int index = data->index;
	struct pool_t *p = get_pool_addr();
	p->longest[index] = data->oldv;
	if (data->newv == 0) {
		// alloc
		while (index) {
			index = PARENT(index);
			int tmp = MAX(p->longest[LEFT_LEAF(index)],
					p->longest[RIGHT_LEAF(index)]);
			if (p->longest[index] == tmp)
				break;
			p->longest[index] = tmp;
		}
	} else {
		// free
		int nodesize = data->newv;
		int left_longest, right_longest;
		while (index) {
			index = PARENT(index);
			nodesize *= 2;

			left_longest = p->longest[LEFT_LEAF(index)];
			right_longest = p->longest[RIGHT_LEAF(index)];

			int tmp;
			if (left_longest + right_longest == nodesize)
				tmp = nodesize;
			else
				tmp = MAX(left_longest, right_longest);
			if (p->longest[index] == tmp)
				break;
			p->longest[index] = tmp;
		}
	}
}

static void nvtxn_record_pool_update(struct nvtxn_info *txn, struct pool_t *p, int index, int newv) {
	if (txn == NULL)
		return;

	struct nvp_t heapnvp = {
		.nvid = get_heap_nvid(),
		.nvoffset = 0,
		.size = get_heap_size()
	};

	struct pool_txn_record_t data = {
		.index = index,
		.oldv = p->longest[index],
		.newv = newv
	};
	int itemoffset = 3 * sizeof(int) + sizeof(int) + sizeof(int) * index;
	nvtxn_record_data_update(txn, NV_HEAP_POOL_UPDATE, heapnvp, itemoffset,
			&data, sizeof(struct pool_txn_record_t));
}

struct nvp_t txn_nvalloc_malloc(struct nvtxn_info *txn, int size) {
	return nvalloc_malloc(txn, size);
}

struct nvp_t nvalloc_malloc(struct nvtxn_info *txn, int size) {
	assert(heap_base_addr != 0);
	// ceil the chunk_num
	double test = (double) size / HEAP_CHUNK_SIZE;
	int chunk_num = (int) test;
	if ((test - chunk_num) > 0) {
		++chunk_num;
	}
	// pool_alloc
	struct pool_t *p = get_pool_addr();
	int index = 0;

	if (chunk_num < 1) {
		chunk_num = 1;
	} else if (!IS_POWER_OF_2(chunk_num)) {
		chunk_num = fixsize(chunk_num);
	}

	if (p->longest[index] < chunk_num) {
		printf("heap memory not enough\n");
		exit(EXIT_FAILURE);
	}
	/* find the match node
	 * add wear-leveling
	 * */
	int nodesize;
	for (nodesize = p->size; nodesize != chunk_num; nodesize /= 2) {
		int li = LEFT_LEAF(index), ri = RIGHT_LEAF(index);
		if (p->longest[li] >= chunk_num && p->longest[ri] >= chunk_num) {
			index = wear_leveling_mgr[li] <= wear_leveling_mgr[ri]? li: ri;
		} else if (p->longest[li] >= chunk_num) {
			index = li;
		} else {
			index = ri;
		}
	}
	/* mark use */
	nvtxn_record_pool_update(txn, p, index, 0);
	p->longest[index] = 0;
	wear_leveling_mgr[index]++;
	int offset = (index + 1) * nodesize - p->size;
	/* record use to parents
	 * update wear-leveling info
	 * */
	int i;
	for (i=PARENT(index); i>0; i=PARENT(i)) {
		int tmp = MAX(p->longest[LEFT_LEAF(i)], p->longest[RIGHT_LEAF(i)]);
		if (p->longest[i] == tmp)
			break;
		p->longest[i] = tmp;
	}
	for (i=PARENT(index); i>0; i=PARENT(i)) {
		int tmp = MAX(wear_leveling_mgr[LEFT_LEAF(i)], wear_leveling_mgr[RIGHT_LEAF(i)]);
		if (wear_leveling_mgr[i] == tmp)
			break;
		wear_leveling_mgr[i] = tmp;
	}

	// return nvp
	struct nvp_t nvp;
	nvp.nvid = get_heap_nvid();
	nvp.nvoffset = offset * HEAP_CHUNK_SIZE + 3 * sizeof(int) + get_pool_size();
	nvp.size = size;
	return nvp;
}

void *nvalloc_getnvp(struct nvp_t *nvp) {
	assert(heap_base_addr != 0);
	if (nvp == NULL) {
		printf("%s: NULL argument\n", __func__);
		return NULL;
	}
	return heap_base_addr + nvp->nvoffset;
}

void txn_nvalloc_free(struct nvtxn_info *txn, struct nvp_t *nvp) {
	return nvalloc_free(txn, nvp);
}

void nvalloc_free(struct nvtxn_info *txn, struct nvp_t *nvp) {
	assert(heap_base_addr != 0);
	int offset = (nvp->nvoffset - 3 * sizeof(int) - get_pool_size())
			/ HEAP_CHUNK_SIZE;
	// pool_free
	struct pool_t *p = get_pool_addr();
	if (offset < 0 || offset >= p->size) {
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
	nvtxn_record_pool_update(txn, p, index, nodesize);
	p->longest[index] = nodesize;
	/* update parent use infomation */
	int left_longest, right_longest;
	while (index) {
		index = PARENT(index);
		nodesize *= 2;

		left_longest = p->longest[LEFT_LEAF(index)];
		right_longest = p->longest[RIGHT_LEAF(index)];

		int tmp;
		if (left_longest + right_longest == nodesize)
			tmp = nodesize;
		else
			tmp = MAX(left_longest, right_longest);
		if (p->longest[index] == tmp)
			break;
		p->longest[index] = tmp;
	}
}

#else
static int hintindex = 0;

static int get_bitmap_bits() {
	assert(heap_base_addr != 0);
	int heap_size = get_heap_size();
	return ((heap_size - 3 * sizeof(int)) * 8) / (HEAP_CHUNK_SIZE * 8 + 1);
}

static void *get_bitmap_addr() {
	assert(heap_base_addr != 0);
	return heap_base_addr + 3 * sizeof(int);
}

static void *get_data_addr() {
	assert(heap_base_addr != 0);
	return heap_base_addr + 3 * sizeof(int) + get_bitmap_bits() / 8;
}

void nvalloc_init(int h_nvid, int heap_size) {
	if (nv_exist(h_nvid) != -1) {
		// nvpcache
		heap_base_addr = nvpcache_search(h_nvid);
		if (heap_base_addr == NULL) {
			heap_base_addr = nv_map(h_nvid);
			nvpcache_insert(h_nvid, heap_base_addr);
		}
		int *magic_ptr = (int *)heap_base_addr;
		if (*magic_ptr != HEAP_MAGIC) {
			printf("NV ALLOC HEAP MAGIC error!\n");
			exit(EXIT_FAILURE);
		}
		return;
	}
	if (heap_size < HEAP_SIZE_MIN) {
		heap_size = HEAP_SIZE_MIN;
	}
	heap_base_addr = nv_get(h_nvid, heap_size);
	// nvpcache
	nvpcache_insert(h_nvid, heap_base_addr);

	int *go_ptr = (int *)heap_base_addr;
	*go_ptr = HEAP_MAGIC;
	++go_ptr;
	*go_ptr = h_nvid;
	++go_ptr;
	*go_ptr = heap_size;
	++go_ptr;
	// set bitmap to 0
	memset((void *)go_ptr, 0, get_bitmap_bits() / 8);
	return;
}

struct nvp_t txn_nvalloc_malloc(struct nvtxn_info *txn, int size) {
	struct nvp_t hnvp;
	hnvp.nvid = get_heap_nvid();
	hnvp.nvoffset = 0;
	hnvp.size = get_heap_size();
	nvtxn_record_data_update(txn, NV_HEAP_BITMAP_UPDATE, hnvp, 3 * sizeof(int), get_bitmap_addr(), get_bitmap_bits()/8);
	return nvalloc_malloc(NULL, size);
}

struct nvp_t nvalloc_malloc(struct nvtxn_info *txn, int size) {
	assert(heap_base_addr != 0);
	if (size < 1)
		size = 1;
	// first fit?
	// ceil the chunk_num
	double test = (double)size / HEAP_CHUNK_SIZE;
	int chunk_num = (int)test;
	if ((test-chunk_num) > 0) {
		++chunk_num;
	}
	int total_chunk_num = get_bitmap_bits();
	char *bitmap_addr = get_bitmap_addr();
	int res_index = -1;
	int i, j;
	// first fit from hintindex
	for (i = hintindex; i <= (total_chunk_num - chunk_num); ++i) {
		for (j = 0; j < chunk_num; ++j) {
			int index = i + j;
			int state = bitmap_addr[index / 8] & (0x1 << (7-index % 8));
			if (state != 0) {
				break;
			} else {
				if (j == (chunk_num-1)) {
					// find
					res_index = i;
				}
			}
		}
		if (res_index != -1) {
			break;
		}
	}

	// a total first fit (slow!!!!)
	if (res_index == -1) {
		hintindex = 0;
		for (i = 0; i <= (total_chunk_num - chunk_num); ++i) {
			for (j = 0; j < chunk_num; ++j) {
				int index = i + j;
				int state = bitmap_addr[index / 8] & (0x1 << (7 - index % 8));
				if (state != 0) {
					break;
				} else {
					if (j == (chunk_num - 1)) {
						// find
						res_index = i;
					}
				}
			}
			if (res_index != -1) {
				break;
			}
		}
	}

	if (res_index == -1) {
		// not find
		printf("heap memory not enough\n");
		exit(EXIT_FAILURE);
	}
	// set bitmap to 1
	for (j=0; j<chunk_num; ++j) {
		int index = res_index + j;
		bitmap_addr[index / 8] |= (0x1 << (7-index % 8));
	}
	hintindex = res_index + chunk_num;
	// return nvp
	struct nvp_t nvp;
	nvp.nvid = get_heap_nvid();
	nvp.nvoffset = res_index * HEAP_CHUNK_SIZE;
	nvp.size = size;
	return nvp;
}

void *nvalloc_getnvp(struct nvp_t *nvp) {
	assert(heap_base_addr != 0);
	if (nvp == NULL) {
		printf("%s: NULL argument\n", __func__);
		return NULL;
	}
	return get_data_addr() + nvp->nvoffset;
}

void txn_nvalloc_free(struct nvtxn_info *txn, struct nvp_t *nvp) {
	struct nvp_t hnvp;
	hnvp.nvid = get_heap_nvid();
	hnvp.nvoffset = 0;
	hnvp.size = get_heap_size();
	nvtxn_record_data_update(txn, NV_HEAP_BITMAP_UPDATE, hnvp, 3 * sizeof(int), get_bitmap_addr(), get_bitmap_bits()/8);
	nvalloc_free(NULL, nvp);
}

void nvalloc_free(struct nvtxn_info *txn, struct nvp_t *nvp) {
	assert(heap_base_addr != 0);

	int res_index = nvp->nvoffset / HEAP_CHUNK_SIZE;
	double test = (double) nvp->size / HEAP_CHUNK_SIZE;
	int chunk_num = (int) test;
	if ((test - chunk_num) > 0) {
		++chunk_num;
	}
	char *bitmap_addr = get_bitmap_addr();
	// set bitmap to 0
	int j;
	for (j = 0; j < chunk_num; ++j) {
		int index = res_index + j;
		bitmap_addr[index / 8] &= ~(0x1 << (7 - index % 8));
	}
}
#endif

struct nvp_t txn_make_nvp_withdata(struct nvtxn_info *txn, void *d, int dsize) {
	assert(heap_base_addr != 0);

	struct nvp_t d_nvp = txn_nvalloc_malloc(txn, dsize);
	void *d_nv = nvalloc_getnvp(&d_nvp);
	memcpy(d_nv, d, dsize);
	return d_nvp;
}

struct nvp_t make_nvp_withdata(void *d, int dsize) {
	assert(heap_base_addr != 0);

	struct nvp_t d_nvp = txn_nvalloc_malloc(NULL, dsize);
	void *d_nv = nvalloc_getnvp(&d_nvp);
	memcpy(d_nv, d, dsize);
	return d_nvp;
}
