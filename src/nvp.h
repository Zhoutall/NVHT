/*
    No-Volatile Pointer
*/
#ifndef __NVP_H__
#define __NVP_H__

#include "rbtree.h"

// this can be stored in NVM
struct nvp_t{
    int nvid;
    int nvoffset;
    int size;
};

// rbtree <nvid, vaddr>
// item in rbtree
// item in DRAM
struct nvpitem {
    struct nvp_t nvp;
    void *vaddr;
    struct rb_node node; /* node in rbtree */
};

void *nvpcache_search(int _nvid);
struct nvpitem *nvpcache_search_foritem(int _nvid);
int nvpcache_insert(int _nvid, int offset, int size, void *addr);
int nvpcache_delete(int _nvid);
static struct nvpitem *nvpcache_search_rb(struct rb_root *root, int _nvid);
static int nvpcache_insert_rb(struct rb_root *root, struct nvpitem *_nvpitem);
static int nvpcache_delete_rb(struct rb_root *root, int _nvid);

/*
 * 1. alloc a new nvp region (get & insert cache)
 * 2. get nvid ptr (a. search fail & attach & insert cache, b. search cache)
 * 3. free a nvp region (remove & delete cache)
 */
struct nvp_t alloc_nvp(int _nvid, int size);
void *get_nvp(struct nvp_t *nvp);
void free_nvp(struct nvp_t *nvp);

/*
 * nv_allocator: like heap malloc allocator, for size < PAGE_SIZE
 */
/*
 * ATTENTION: call this on the top main func
 * h_nvid: nvid for heap
 * size: heap size
 */
void nvalloc_init(int h_nvid, int size);
struct nvp_t nvalloc_malloc(int size);
inline void *nvalloc_getnvp(struct nvp_t *nvp);
void nvalloc_free(struct nvp_t *nvp);
/*
 * create nvp and full with data (raw data, so that no pointer)
 */
struct nvp_t make_nvp_withdata(void *d, int dsize);
#endif
