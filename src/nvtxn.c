#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nvtxn.h"
#include "util.h"
#include "nvsim.h"

static struct nvp_t nvp_parse(struct nvtxn_record_header *h) {
	struct nvp_t ret;
	ret.nvid = h->nvp.nvid;
	ret.nvoffset = h->nvp.nvoffset;
	ret.size = h->nvp.size;
	return ret;
}

struct nvtxn_info nvtxn_start(struct nvl_header *nvlh) {
	// check nvl_header valid
	if (!nvl_header_valid(nvlh)) {
		printf("NVLOGGER header magic number error\n");
		exit(EXIT_FAILURE);
	}
	struct nvtxn_info ret;
	ret.nvlh = nvlh;
	ret.txn_id = random_txnid();
	return ret;
}

void nvtxn_record_nv_update(struct nvtxn_info *txn, NVTXN_OP_T op, int nvid) {
	struct nvtxn_record_header rh = {
		.txn_id = txn->txn_id,
		.op = op,
		.nvp.nvid = nvid
	};
	nvl_append(txn->nvlh, &rh, sizeof(struct nvtxn_record_header));
}

void nvtxn_record_data_update(struct nvtxn_info *txn, NVTXN_OP_T op,
		struct nvp_t nvp, int offset, void *undodata, int dsize) {
	struct nvtxn_record_header rh = {
		.txn_id = txn->txn_id,
		.op = op,
		.nvp.nvid = nvp.nvid,
		.nvp.nvoffset = nvp.nvoffset,
		.nvp.size = nvp.size,
		.offset = offset,
		.dsize = dsize
	};
	nvl_txn_append(txn->nvlh, &rh, sizeof(struct nvtxn_record_header), undodata, dsize);
}

void nvtxn_commit(struct nvtxn_info *txn) {
	struct nvtxn_record_header rh;
	rh.txn_id = txn->txn_id;
	rh.op = COMMIT;
	// write commit log
	nvl_append(txn->nvlh, &rh, sizeof(struct nvtxn_record_header));
	// clear log (Restriction: no txn concurrency)
	nvl_reset(txn->nvlh);
}

void nvtxn_recover(struct nvl_header *nvlh) {
	struct nvl_record *it = nvl_begin(nvlh);
	struct nvtxn_record_header *d;
	if (it == NULL) return;
	// TODO check txn_id
	// TODO buffer is 1000...
	struct nvl_record *logs[1000];
	int i = 0;
	// find whether the last one is commit
	while(it != NULL) {
		logs[i++] = it;
		it = nvl_next(nvlh, it);
	}
	--i;
	d = (struct nvtxn_record_header *)logs[i]->data;
	if (d->op == COMMIT) {
		nvl_reset(nvlh);
		return;
	}
	// do the recover (reverse undo)
	for (;i>=0; --i) {
		char *data = logs[i]->data;
		d = (struct nvtxn_record_header *)data;
		char *addr;
		if (d->op == NV_HEAP_DATA) {
			struct nvp_t tmp = nvp_parse(d);
			addr = nvalloc_getnvp(&tmp);
			addr += d->offset;
			memcpy(addr, data + sizeof(struct nvtxn_record_header), d->dsize);
		} else if (d->op == NVHT_HEADER || d->op == NVHT_PUT || d->op == NVHT_REMOVE ||
				d->op == NV_HEAP_BITMAP_UPDATE || d->op == NV_DATASET) {
			struct nvp_t tmp = nvp_parse(d);
			addr = get_nvp(&tmp);
			addr += d->offset;
			memcpy(addr, data + sizeof(struct nvtxn_record_header), d->dsize);
		} else if (d->op == NV_ALLOC) {
			// free nvid
			int nvid = d->nvp.nvid;
			nv_remove(nvid);
		} else if (d->op == NV_FREE) {
			// just do nothing
		} else {
			printf("OP not support when recovering\n");
			exit(EXIT_FAILURE);
		}
	}
	nvl_reset(nvlh);
}
