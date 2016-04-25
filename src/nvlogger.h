#ifndef __NVLOGGER_H__
#define __NVLOGGER_H__

#include "nvp.h"
/*
 * TODO
 * implemented as a ring buffer
 * with variable (not fixed) log record size
 *
 * Key points:
 * 	cannot overlap uncommit log (adjust r_offset when commit)
 * 	variable length record
 *
 * 	| meta-data | buffer region |
 */

#define NVLOGGER_DEFAULT_SIZE 1048576 // 1M
#define NVLOGGER_MAGIC_0 0xB3333A55 /* header */
#define NVLOGGER_MAGIC_1 0xB3CA5C3B /* record start */
#define NVLOGGER_MAGIC_2 0xA1C1BCA1 /* record end */

struct nvl_header {
	int magic0;
    int size; // total size (total offset, exclude the header)
    int w_offset; // write offset
    char buffer[0];
};

struct nvl_record {
	int magic1; /* NVLOGGER_MAGIC_1 */
	int len; /* length of data (exclude NVLOGGER_MAGIC_2) */
	char data[0]; /* data will be end with NVLOGGER_MAGIC_2 */
};

/*
 * chech magic number for header
 */
int nvl_header_valid(struct nvl_header *h);
/*
 * re-attach or alloc nvlogger space
 */
struct nvl_header *nvl_get(int nvid, int size);

void nvl_free(int nvid);
/*
 * data should be end with NVLOGGER_MAGIC_2
 */
void nvl_txn_append(struct nvl_header *nvl, void *data_header, int dhsize, void *data, int dsize);
void nvl_append(struct nvl_header *nvl, void *data, int dsize);
void nvl_reset(struct nvl_header *nvl);
/*
 * iterator for nvlogger
 * return NULL if empty
 */
struct nvl_record *nvl_begin(struct nvl_header *nvl);
/*
 * return NULL if reaching the end or reaching a broken log record
 */
struct nvl_record *nvl_next(struct nvl_header *nvl, struct nvl_record *now);
#endif
