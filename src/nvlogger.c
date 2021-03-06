#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nvlogger.h"
#include "nvp.h"
#include "nvsim.h"

static inline int get_nvlogger_size(struct nvl_header *h) {
	return h->size + sizeof(struct nvl_header);
}

static struct nvl_header *snvlh = NULL;

int nvl_header_valid(struct nvl_header *h) {
	return h->magic0 == NVLOGGER_MAGIC_0;
}

struct nvl_header *nvl_get(int nvid, int size) {
	if (size < NVLOGGER_DEFAULT_SIZE) {
		size = NVLOGGER_DEFAULT_SIZE;
	}
	if (snvlh != NULL) return snvlh;
	struct nvl_header *nvlogger_addr;
	if (nv_exist(nvid) != -1) {
		nvlogger_addr = nvpcache_search(nvid);
		if (nvlogger_addr == NULL) {
			nvlogger_addr = nv_map(nvid);
			nvpcache_insert(nvid, nvlogger_addr);
		}
		if (nvlogger_addr->magic0 != NVLOGGER_MAGIC_0) {
			printf("NVLOGGER header magic number error\n");
			// crash when init
			nvlogger_addr->size = size - sizeof(struct nvl_header);
			nvlogger_addr->w_offset = 0;
			nvlogger_addr->magic0 = NVLOGGER_MAGIC_0;
		}
		snvlh = nvlogger_addr;
		return nvlogger_addr;
	}

	nvlogger_addr = nv_get(nvid, size);
	nvpcache_insert(nvid, nvlogger_addr);
	nvlogger_addr->size = size - sizeof(struct nvl_header);
	nvlogger_addr->w_offset = 0;
	nvlogger_addr->magic0 = NVLOGGER_MAGIC_0;
	snvlh = nvlogger_addr;
	return nvlogger_addr;
}

void nvl_free(int nvid) {
	nv_remove(nvid);
}

/*
 * nvl_append with data header (for txn header)
 */
void nvl_txn_append(struct nvl_header *nvl, void *data_header, int dhsize, void *data, int dsize) {
	int realsize = sizeof(struct nvl_record) + dhsize + dsize + sizeof(int);
	/*
	 * check memory enough
	 */
	if (nvl->size - nvl->w_offset < realsize) {
		printf("NVLOGGER space not enough\n");
		exit(EXIT_FAILURE);
	}
	struct nvl_record *pos = (struct nvl_record *) (nvl->buffer + nvl->w_offset);
	pos->magic1 = NVLOGGER_MAGIC_1;
	pos->len = dhsize + dsize;
	memcpy(pos->data, data_header, dhsize);
	if (data != NULL) {
		memcpy(pos->data + dhsize, data, dsize);
	}
	int *magic2 = (int *) (pos->data + dhsize + dsize);
	*magic2 = NVLOGGER_MAGIC_2;
	/* finish append */
	nvl->w_offset += realsize;
}

void nvl_append(struct nvl_header *nvl, void *data, int dsize) {
	int realsize = sizeof(struct nvl_record) + dsize + sizeof(int);
	/*
	 * check memory enough
	 */
	if (nvl->size - nvl->w_offset < realsize) {
		printf("NVLOGGER space not enough\n");
		exit(EXIT_FAILURE);
	}
	struct nvl_record *pos = (struct nvl_record *) (nvl->buffer + nvl->w_offset);
	pos->magic1 = NVLOGGER_MAGIC_1;
	pos->len = dsize;
	memcpy(pos->data, data, dsize);
	int *magic2 = (int *) (pos->data + dsize);
	*magic2 = NVLOGGER_MAGIC_2;
	/* finish append */
	nvl->w_offset += realsize;
}

void nvl_reset(struct nvl_header *nvl) {
	nvl->w_offset = 0;
}

struct nvl_record *nvl_begin(struct nvl_header *nvl) {
	// check empty
	if (nvl->w_offset == 0)
		return NULL;
	struct nvl_record *begin = (struct nvl_record *) nvl->buffer;
	// check magic1, magic2
	if (begin->magic1 != NVLOGGER_MAGIC_1)
		return NULL;
	int *magic2 = (int *) (begin->data + begin->len);
	if (*magic2 != NVLOGGER_MAGIC_2)
		return NULL;
	return begin;
}

struct nvl_record *nvl_next(struct nvl_header *nvl, struct nvl_record *now) {
	/* check have more space from magic1 to magic2 */
	if ((nvl->size - nvl->w_offset)
			< (sizeof(struct nvl_record) + sizeof(int))) {
		return NULL;
	}
	struct nvl_record *next = (struct nvl_record *) ((char *) now
			+ sizeof(struct nvl_record) + now->len + sizeof(int));
	if ((void *)next >= (void *)(nvl->buffer+nvl->w_offset))
		return NULL;
	if (next->magic1 != NVLOGGER_MAGIC_1)
		return NULL;
	int *magic2 = (int *) (next->data + next->len);
	// cannot find magic2 means crash when writing log
	if (*magic2 != NVLOGGER_MAGIC_2)
		return NULL;
	return next;
}
