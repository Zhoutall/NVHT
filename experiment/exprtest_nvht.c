#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include "nvht.h"

#define ID1 1234
#define ID2 5678

#define KEYSTR "test-key-[%d]"
#define VALUESTR "test-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-value-[%d]"

void test_insert(int count) {
	int i;
	long long t1, t2;
	nvalloc_init(ID1, 500000000);
	NVHT *h = nvht_init(ID2);
	i = 0;
	t1 = ustime();
	while(i++ < count) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		nvht_put(h, k, strlen(k) + 1, v, strlen(v) + 1);
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__,  t2 - t1);
}

void test_insertr(int count) {
	int i, j;
	long long t1, t2;
	nvalloc_init(ID1, 500000000);
	NVHT *h = nvht_init(ID2);
	i = 0, j = count-1;
	t1 = ustime();
	while(i<j) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		nvht_put(h, k, strlen(k) + 1, v, strlen(v) + 1);
		if (i>=j) break;
		sprintf(k, KEYSTR, j);
		sprintf(v, VALUESTR, j);
		nvht_put(h, k, strlen(k) + 1, v, strlen(v) + 1);
		++i;
		--j;
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__,  t2 - t1);
}

void test_search(int count) {
	int i;
	long long t1, t2;
	nvalloc_init(ID1, 500000000);
	NVHT *h = nvht_init(ID2);
	i = 0;
	t1 = ustime();
	while (i++ < count) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		nvht_get(h, k, strlen(k) + 1, v);
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__, t2 - t1);
}

void test_searchr(int count) {
	int i, j;
	long long t1, t2;
	nvalloc_init(ID1, 500000000);
	NVHT *h = nvht_init(ID2);
	i = 0, j = count - 1;
	t1 = ustime();
	while (i < j) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		nvht_get(h, k, strlen(k) + 1, v);
		if (i >= j)
			break;
		sprintf(k, KEYSTR, j);
		nvht_get(h, k, strlen(k) + 1, v);
		++i;
		--j;
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__,  t2 - t1);
}

void test_del(int count) {
	int i;
	long long t1, t2;
	nvalloc_init(ID1, 500000000);
	NVHT *h = nvht_init(ID2);
	i = 0;
	t1 = ustime();
	while (i++ < count) {
		char k[30];
		sprintf(k, KEYSTR, i);
		nvht_remove(h, k, strlen(k) + 1);
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__, t2 - t1);
}

void clean() {
	struct nvp_t tmp;
	tmp.nvid = ID2;
	tmp.nvoffset = 0;
	tmp.size = 0;
	nvht_free(get_nvp(&tmp));
	nv_remove(ID1);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		clean();
		return -1;
	}
	int count = atoi(argv[2]);
	printf("count %d\n", count);
	if (strcmp(argv[1], "insert")==0) {
		test_insert(count);
		clean();
	} else if (strcmp(argv[1], "insertr")==0) {
		test_insertr(count);
		clean();
	} else if (strcmp(argv[1], "load")==0) {
		test_insert(count);
	} else if (strcmp(argv[1], "search")==0) {
		test_search(count);
	} else if (strcmp(argv[1], "searchr")==0) {
		test_searchr(count);
	} else if (strcmp(argv[1], "delete")==0) {
		test_del(count);
	} else {
		printf("No test for %s\n", argv[1]);
	}
	return 0;
}
