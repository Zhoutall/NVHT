/*
 * simulate nv system api
*/
#ifndef __NVSIM_H__
#define __NVSIM_H__

#include <stdlib.h>
#include "nvp.h"

/*
 * alloc a new nv region and attach (attach an NEW nv region to vm)
 * return v_addr
 */
void *nv_get(int64_t nvid, int size);
/*
 * attach an EXISTING nv region to vm
 * return v_addr
 */
void *nv_map(int nvid);
/*
 * detach an existing nv region to vm
 * MAY NOT BE USED in this proj
 */
int nv_detach(void *nvaddr);
/*
 * remove an existing nv region
 */
int nv_remove(int nvid);
/*
 * check nvid existence
 * if not return -1 else shmid
 */
int nv_exist(int nvid);
#endif
