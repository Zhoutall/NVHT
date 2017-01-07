#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <vector>
#include <deque>
#include <iostream>

using namespace std;

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))

struct pool_t {
	int size;
	bool do_wear_leveling;
	int longest[0];
};

static void *heap_base_addr = 0;
static int *wear_leveling_mgr = 0; // wear-leveling

#define HEAP_SIZE_MIN 131072 // 128kB
#define HEAP_CHUNK_SIZE 128 // 128B per chunk
#define HEAP_MAGIC 0x5A1AA1A5

unsigned fixsize(unsigned size) {
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;
	return size + 1;
}

static int get_heap_size() {
	// assert(heap_base_addr != 0);
	int *hsize_ptr = (int *)heap_base_addr + 2;
	return *hsize_ptr;
}

static int get_heap_nvid() {
	// assert(heap_base_addr != 0);
	int *hvid_ptr = (int *)heap_base_addr + 1;
	return *hvid_ptr;
}

static struct pool_t *get_pool_addr() {
	// assert(heap_base_addr != 0);
	return (struct pool_t *)((int *)heap_base_addr + 3);
}

static int get_pool_size() {
	// assert(heap_base_addr != 0);
	struct pool_t *p = get_pool_addr();
	return 2 * p->size * sizeof(int);
}

static void nvalloc_init_core(int h_nvid, int heap_size, int realsize, bool do_wl) {
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
	pool->do_wear_leveling = do_wl;
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

void print_wear_leveling_stat() {
	// Maximum block erase count. 
	struct pool_t *p = get_pool_addr();
	cout << p->do_wear_leveling << " " << wear_leveling_mgr[0] << endl;
}

void nvalloc_init(int h_nvid, int heap_size, bool do_wl) {
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
	heap_base_addr = malloc(heap_size);
	nvalloc_init_core(h_nvid, heap_size, realsize, do_wl);
	wear_leveling_init(get_heap_size());
	return;
}

void nvalloc_delete() {
	free(heap_base_addr);
	free(wear_leveling_mgr);
}

int nvalloc_malloc(int size) {
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
			if (p->do_wear_leveling)
				index = wear_leveling_mgr[li] <= wear_leveling_mgr[ri]? li: ri;
			else
				index = li;
		} else if (p->longest[li] >= chunk_num) {
			index = li;
		} else {
			index = ri;
		}
	}
	/* mark use */
	p->longest[index] = 0;
	wear_leveling_mgr[index]++;
	int offset = (index + 1) * nodesize - p->size;
	/* record use to parents
	 * update wear-leveling info
	 * */
	int i;
	for (i=PARENT(index); i>=0; i=PARENT(i)) {
		int tmp = MAX(p->longest[LEFT_LEAF(i)], p->longest[RIGHT_LEAF(i)]);
		if (p->longest[i] == tmp)
			break;
		p->longest[i] = tmp;
		if (i==0)
			break;
	}
	
	for (i=PARENT(index); i>=0; i=PARENT(i)) {
		int tmp = MAX(wear_leveling_mgr[LEFT_LEAF(i)], wear_leveling_mgr[RIGHT_LEAF(i)]);
		if (wear_leveling_mgr[i] >= tmp) {
			break;
		}
		wear_leveling_mgr[i] = tmp;
		if (i==0)
			break;
	}

	return offset;
}

void nvalloc_free(int offset) {
	assert(heap_base_addr != 0);
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

int main(int argc, char *argv[]) {
	// gen operation seqence
	// alloc [128b, 512b, 1kb, 32kb, 128kb]
	if (argc < 3) 
		return -1;
	int do_times = atoi(argv[1]);
	int free_delay = atoi(argv[2]);
	int ranv[] = {128, 512, 1024, 32*1024, 128*1024};
	vector<int> allocv;
	for (int i=0; i<(do_times/5); ++i)
	{
		allocv.push_back(ranv[rand()%5]);
		allocv.push_back(ranv[rand()%3]);
		allocv.push_back(ranv[rand()%2]);
		allocv.push_back(ranv[rand()%1]);
		allocv.push_back(ranv[rand()%1]);
	}
	deque<int> freev;

	nvalloc_init(1, 10*1024*1024, false);
	int offset;
	for (int i=0; i<allocv.size(); ++i) {
		offset = nvalloc_malloc(allocv[i]);
		freev.push_front(offset);
		if (i >= free_delay)
		{
			nvalloc_free(freev.back());
			freev.pop_back();
		}
	}
	print_wear_leveling_stat();
	nvalloc_delete();
	nvalloc_init(1, 10*1024*1024, true);
	for (int i=0; i<allocv.size(); ++i) {
		offset = nvalloc_malloc(allocv[i]);
		freev.push_front(offset);
		if (i >= free_delay)
		{
			nvalloc_free(freev.back());
			freev.pop_back();
		}
	}
	print_wear_leveling_stat();
	nvalloc_delete();
	return 0;
}