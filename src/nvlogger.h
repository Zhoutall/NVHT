#ifndef __NVLOGGER_H__
#define __NVLOGGER_H__

#include "nvp.h"
/*
 * It is a ring buffer
 */

#define NVLOGGER_DEFAULT_SIZE (1024*1024)

struct nvlogger_header {
    int size; // total size (total offset)
    int r_offset; // read offset
    int w_offset; // write offset
    char data[0];
};

nvp nvlogger_init(int nvid);

int nvlogger_clear(nvlogger_header *h);

int nvlogger_append(void *data, int size);

#endif
