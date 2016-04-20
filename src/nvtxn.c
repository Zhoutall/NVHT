#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nvtxn.h"
#include "util.h"

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

void nvtxn_record(struct nvtxn_info *txn, NVTXN_OP_T op, struct nvp_t nvp, int offset, void *undodata, int dsize) {
	void *buffer = malloc(sizeof(struct nvtxn_record_header) + dsize);
	struct nvtxn_record_header rh;
	rh.txn_id = txn;
	rh.op = op;
	rh.nvp = nvp;
	rh.offset = offset;
	rh.dsize = dsize;
	memcpy(buffer, rh, sizeof(struct nvtxn_record_header));
	memcpy(buffer + sizeof(struct nvtxn_record_header), undodata, dsize);
	nvl_append(txn->nvlh, buffer, sizeof(struct nvtxn_record_header) + dsize);
	free(buffer);
}

void nvtxn_commit(struct nvtxn_info *txn) {
	void *buffer = malloc(sizeof(struct nvtxn_record_header));
	struct nvtxn_record_header rh;
	rh->txn_id = txn->txn_id;
	rh->op = COMMIT;
	// write commit log
	nvl_append(txn->nvlh, buffer, sizeof(struct nvtxn_record_header));
	free(buffer);
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
	d = logs[i]->data;
	if (d->op == COMMIT) {
		nvl_reset(nvlh);
		return;
	}
	// do the recover (reverse undo)
	for (;i>=0; --i) {
		char *data = logs[i]->data;
		d = data;
		char *addr;
		if (d->op == NVHT_PUT || d->op == NVHT_REMOVE) {
			addr = nvalloc_getnvp(d->nvp);
			addr += d->offset;
			memcpy(addr, data + sizeof(struct nvtxn_record_header), d->dsize);
		} else {
			printf("OP not found when recovering\n");
			exit(EXIT_FAILURE);
		}
	}
	nvl_reset(nvlh);
}
