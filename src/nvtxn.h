#ifndef __NVTXN_H__
#define __NVTXN_H__

#include "nvlogger.h"
#include "nvp.h"

struct nvp_t;

/*
 * trasaction module
 * undo log, record the old value of a nvp
 *
 * now txn_id is unused, assume no concurrent transaction
 */

/*
 * op will write to log, so this enum only support append (keep index unchanged)
 */
typedef enum {
	NVHT_HEADER,
	NVHT_PUT,
	NVHT_REMOVE,
	NVHT_FREE, /* TODO */
	NV_ALLOC,
	NV_FREE,
	NV_DATASET,
	NV_HEAP_DATA,
	NV_HEAP_BITMAP_UPDATE,
	COMMIT
} NVTXN_OP_T;

struct nvtxn_info {
	int txn_id;
	struct nvl_header *nvlh;
};

/*
 * for multiple usage
 */
struct nvtxn_record_header {
	int txn_id;
	NVTXN_OP_T op;
	struct {
	    int nvid;
	    int nvoffset;
	    int size;
	} nvp;
	int offset;
	int dsize;
};

/*
 * return a unique transaction id
 */
struct nvtxn_info nvtxn_start(struct nvl_header *nvlh);

void nvtxn_record_nv_update(struct nvtxn_info *txn, NVTXN_OP_T op, int nvid);
/*
 * arguments:
 * 	txn: transaction infomation
 * 	op: enum op
 * 	nvp: nv region for undo
 * 	offset: offset of nvp
 * 	undodata: undo data
 * 	dsize: undodata size
 */
void nvtxn_record_data_update(struct nvtxn_info *txn, NVTXN_OP_T op,
		struct nvp_t nvp, int offset, void *undodata, int dsize);

void nvtxn_commit(struct nvtxn_info *txn);

void nvtxn_recover(struct nvl_header *nvlh);
#endif
