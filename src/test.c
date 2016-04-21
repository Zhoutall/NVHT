/* test code */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "nvsim.h"
#include "list.h"
#include "nvp.h"
#include "util.h"
#include "nvht.h"
#include "nvlogger.h"
#include "nvtxn.h"

void nvsim_test1() {
	int nvid = 1234;
	char *buf = nv_get(nvid, 4096);
	strcpy(buf, "hello nvsim");
	printf("vaddr: %p, shm data: %s\n", (void *)buf, buf);
	nv_detach(buf);
}

void nvsim_test2() {
	int nvid = 1234;
	char *buf = nv_attach(nvid);
	printf("vaddr: %p, shm data: %s\n", (void *)buf, buf);
	nv_detach(buf);
}

void nvsim_test3() {
	int nvid = 1234;
	int ret = nv_exist(nvid);
	printf("exist: %d\n", ret);
	nv_remove(nvid);
	ret = nv_exist(nvid);
	printf("exist: %d\n", ret);
}

void list_test() {
	struct item {
		int data;
		struct list_head list;
	};

	struct item *itemhead = malloc(sizeof(struct item));
	INIT_LIST_HEAD(&itemhead->list);
	int i=0;
	while(i!=10) {
		struct item *tmp = malloc(sizeof(struct item));
		tmp->data = i;
		list_add(&tmp->list, &itemhead->list);
		++i;
	}
	while(list_empty(&itemhead->list) != 1) {
		struct item *entry;
		entry = list_first_entry(&itemhead->list, struct item, list);
		printf("item data %d\n", entry->data);
		list_del(&entry->list);
		free(entry);
	}
	free(itemhead);
}

struct testa_s {
	int data;
	struct nvp_t nvp_test;
};

void nvp_test1() {
	/*
	 * new two regions, first one with a nvp to second one
	 */
	int nvid_a = 77677, nvid_b = 88688;
	int size = 4096;
	struct nvp_t nvp_a = alloc_nvp(nvid_a, size);
	struct nvp_t nvp_b = alloc_nvp(nvid_b, size);
	struct testa_s *buf_a = get_nvp(&nvp_a);
	buf_a->data = 1234;
	buf_a->nvp_test = nvp_b;
	printf("nv region a: %p, data: %d\n", buf_a, buf_a->data);
	char *buf_b = get_nvp(&nvp_b);
	strcpy(buf_b, "This is data in nv region b");
	printf("nv region b: %p, data: %s\n", buf_b, buf_b);
}

void nvp_test2() {
	/*
	 * get first one and use nvp to get second one
	 */
	int nvid_a = 77677;
	struct nvp_t nvp_a;
	nvp_a.nvid = nvid_a;
	nvp_a.nvoffset = 0;
	nvp_a.size = 4096;
	struct testa_s *buf_a = get_nvp(&nvp_a);
	printf("nv region a: %p, data: %d\n", buf_a, buf_a->data);
	char *buf_b = get_nvp(&buf_a->nvp_test);
	printf("nv region b: %p, data: %s\n", buf_b, buf_b);

	struct testa_s *buf_a2 = get_nvp(&nvp_a);
	printf("nv region a (get again): %p, data: %d\n", buf_a2, buf_a2->data);
	// IMPORTANT: free order
	free_nvp(&buf_a->nvp_test);
	free_nvp(&nvp_a);
}

void nvalloc_test1() {
	struct nvp_t nvp_a = alloc_nvp(77677, 4096);
	struct testa_s *buf_a = get_nvp(&nvp_a);
	nvalloc_init(12345, 0);
	struct nvp_t nvp1 = nvalloc_malloc(10);
	struct nvp_t nvp2 = nvalloc_malloc(300);
	struct nvp_t nvp3 = nvalloc_malloc(300);
	nvalloc_free(&nvp2);
	struct nvp_t nvp4 = nvalloc_malloc(400);
	struct nvp_t nvp5 = nvalloc_malloc(200);
	char *addr = nvalloc_getnvp(&nvp4);
	strcpy(addr, "This is data in nvalloc_malloc nvp4");
	buf_a->data = 1234;
	buf_a->nvp_test = nvp4;
}

void nvalloc_test2() {
	int nvid_a = 77677;
	struct nvp_t nvp_a;
	nvp_a.nvid = nvid_a;
	nvp_a.nvoffset = 0;
	nvp_a.size = 4096;
	struct testa_s *buf_a = get_nvp(&nvp_a);
	printf("nv region a: %p, data: %d\n", buf_a, buf_a->data);
	nvalloc_init(12345, 0);
	char *addr = nvalloc_getnvp(&buf_a->nvp_test);
	printf("nv region b: %p, data: %s\n", addr, addr);
	struct nvp_t nvp_h = nvalloc_malloc(1000);
}

void nvalloc_clear() {
	nv_remove(12345);
	nv_remove(77677);
}

void util_test() {
	int i=0;
	while(i<10) {
		int r = random_nvid();
		printf("random int: %d, k: %d\n", r, ftok("/tmp", r));
		++i;
	}
}

void nvht_txn_test() {
	nvalloc_init(12345, 6000000); // 6M
	struct nvp_t ht_nvp = nvht_init(9988);
	struct nvht_header *h = get_nvp(&ht_nvp);
	printf("size: %d %d\n", h->size,  h->capacity);
	int i = 0;
	while (++i < 10) {
		char k[20];
		char v[20];
		sprintf(k, "nv key %d", i);
		sprintf(v, "nv value %d", i * i);
		nvht_put(ht_nvp, k, strlen(k) + 1, v, strlen(v) + 1);
	}

//	nvht_rehash(ht_nvp);
//	i = 0;
//	printf("--------\n");
//	while (++i < 10) {
//		char k[20];
//		sprintf(k, "nv key %d", i);
//		struct nvp_t *tmp = nvht_get(ht_nvp, k, strlen(k) + 1);
//		printf("%s: %s\n", k, (char *) nvalloc_getnvp(tmp));
//	}
//	printf("size: %d %d\n", h->size, h->capacity);
//
//	return; /*****/

	printf("size: %d\n", h->size);
	i = 0;
	printf("--------\n");
	while (++i < 10) {
		char k[20];
		sprintf(k, "nv key %d", i);
		nvht_remove(ht_nvp, k, strlen(k) + 1);
	}
	printf("size: %d\n", h->size);
	i = 0;
	printf("--------\n");
	while (++i < 10) {
		char k[20];
		sprintf(k, "nv key %d", i);
		struct nvp_t *tmp = nvht_get(ht_nvp, k, strlen(k) + 1);
		printf("%s: %s\n", k, (char *) nvalloc_getnvp(tmp));
	}
	printf("size: %d\n", h->size);
}

void nvht_test1() {
	nvalloc_init(12345, 6000000); // 6M
	struct nvp_t ht_nvp = nvht_init(9988);
	struct nvht_header *h = get_nvp(&ht_nvp);
	printf("capacity: %d\n", h->capacity);
	// try put many data
	int i=0;
	while (++i < 300) {
		char k[20];
		char v[20];
		sprintf(k, "nv key %d", i);
		sprintf(v, "nv value %d", i*i);
		nvht_put(ht_nvp, k, strlen(k)+1, v, strlen(v)+1);
	}
	char k1[] = "key 1";
	char v1[] = "value 1";
	nvht_put(ht_nvp, k1, sizeof(k1), v1, sizeof(v1));
	printf("--------\n");
	i = 0;
	while (++i < 300) {
		char k[20];
		sprintf(k, "nv key %d", i);
		struct nvp_t *tmp = nvht_get(ht_nvp, k, strlen(k) + 1);
		printf("%s: %s\n", k, (char *) nvalloc_getnvp(tmp));
	}
}

void nvht_test2() {
	nvalloc_init(12345, 6000000);
	struct nvp_t ht_nvp = nvht_init(9988);
	struct nvht_header *h = get_nvp(&ht_nvp);
	printf("capacity: %d\n", h->capacity);
	char k1[] = "key 1";
	struct nvp_t *res = nvht_get(ht_nvp, k1, sizeof(k1));
	printf("Data: %s\n", (char *)nvalloc_getnvp(res));
	nvht_remove(ht_nvp, k1, sizeof(k1));
	res = nvht_get(ht_nvp, k1, sizeof(k1));
	printf("Must NULL %d\n", res == NULL);
	printf("capacity: %d\n", h->capacity);
	int i = 0;
	while (++i < 300) {
		char k[20];
		sprintf(k, "nv key %d", i);
		struct nvp_t *tmp = nvht_get(ht_nvp, k, strlen(k)+1);
		printf("%s: %s\n", k,
				(char *) nvalloc_getnvp(tmp));
	}
}

void nvht_clear() {
	struct nvp_t tmp;
	tmp.nvid = 9988;
	tmp.nvoffset = 0;
	tmp.size = 0;
	nvht_free(tmp);
	nv_remove(12345);
}

void nvlogger_test1() {
	struct nvl_header *nvlh = nvl_get(9966, 0);
	char data1[] = "OP1#hello data#111";
	nvl_append(nvlh, data1, sizeof(data1));
	char data2[] = "OP2#hello data#222";
	nvl_append(nvlh, data2, sizeof(data2));
	struct nvl_record *it = nvl_begin(nvlh);
	while (it != NULL) {
		char tmp[10];
		int a, b;
		sscanf(it->data, "OP%d#%[^#]#%d", &b, tmp, &a);
		printf("%s OP %d %d\n", tmp, b, a);
		it = nvl_next(nvlh, it);
	}
}

void nvlogger_clear() {
	nv_remove(9966);
}

void nvtxn_test1() {
	nvalloc_init(12345, 0);
	struct nvl_header *nvlh = nvl_get(9966, 0);
	struct nvp_t nvp_a = alloc_nvp(77677, 4096);
	char *buf_a = get_nvp(&nvp_a);
	strcpy(buf_a, "buf_a status 1");
	struct nvp_t nvp_b = nvalloc_malloc(40);
	char *buf_b = nvalloc_getnvp(&nvp_b);
	strcpy(buf_b, "buf_b status 11");

	struct nvtxn_info txn = nvtxn_start(nvlh);
	nvtxn_record_data_update(&txn, NV_DATASET, nvp_a, 0, buf_a, 4096);
	strcpy(buf_a, "buf_a new status 2");
	printf("%s\n", buf_a);
	nvtxn_record_data_update(&txn, NVHT_PUT, nvp_b, 0, buf_b, 40);
	strcpy(buf_b, "buf_b new status 22");
	printf("%s\n", buf_b);
//	nvtxn_commit(&txn);
	nvtxn_recover(nvlh);
	printf("%s\n", buf_a);
	printf("%s\n", buf_b);
}

void nvtxn_test2() {
	struct nvl_header *nvlh = nvl_get(9966, 0);
	struct nvtxn_info txn = nvtxn_start(nvlh);
	nvtxn_record_nv_update(&txn, NV_ALLOC, 77677);
	struct nvp_t nvp_a = alloc_nvp(77677, 4096);
//	nvtxn_commit(&txn);
	nvtxn_recover(nvlh);
	nv_remove(9966);
}

void nvtxn_clear() {
	nv_remove(9966);
	nv_remove(77677);
	nv_remove(12345);
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		return -1;
	}
	if (strcmp(argv[1], "nvsim1") == 0) {
		nvsim_test1();
	} else if (strcmp(argv[1], "nvsim2") == 0) {
		nvsim_test2();
	} else if (strcmp(argv[1], "nvsim3") == 0) {
		nvsim_test3();
	} else if (strcmp(argv[1], "list") == 0) {
		list_test();
	} else if (strcmp(argv[1], "nvp1") == 0) {
		nvp_test1();
	} else if (strcmp(argv[1], "nvp2") == 0) {
		nvp_test2();
	} else if (strcmp(argv[1], "nvalloc1") == 0) {
		nvalloc_test1();
	} else if (strcmp(argv[1], "nvalloc2") == 0) {
		nvalloc_test2();
	} else if (strcmp(argv[1], "nvalloc3") == 0) {
		nvalloc_clear();
	} else if (strcmp(argv[1], "util") == 0) {
		util_test();
	} else if (strcmp(argv[1], "nvht1") == 0) {
		nvht_test1();
	} else if (strcmp(argv[1], "nvht2") == 0) {
		nvht_test2();
	} else if (strcmp(argv[1], "nvht_c") == 0) {
		nvht_clear();
	} else if (strcmp(argv[1], "nvl1") == 0) {
		nvlogger_test1();
	} else if (strcmp(argv[1], "nvl_c") == 0) {
		nvlogger_clear();
	} else if (strcmp(argv[1], "nvtxn1") == 0) {
		nvtxn_test1();
	} else if (strcmp(argv[1], "nvtxn2") == 0) {
		nvtxn_test2();
	} else if (strcmp(argv[1], "nvtxn_c") == 0) {
		nvtxn_clear();
	} else if (strcmp(argv[1], "nvht_txn") == 0) {
		nvht_txn_test();
	} else {
		printf("No test for %s\n", argv[1]);
	}
}
