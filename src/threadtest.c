#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include "nvht.h"

#define ID1 1345
#define ID2 5785

#define MAXTHREADNUM 20
#define TOTAL 1000000
#define TOTALWRITE 300000
#define TOTALSEARCH 700000

#define KEYSTR "test-key-[%d]"
#define VALUESTR "test-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-value-[%d]"

NVHT *h;
int thread_num;

void *insert(void *arg) {
	int i;
	int pos = (int) arg;

	int start = TOTAL*pos/thread_num;
	int end = TOTAL*(pos+1)/thread_num;
	printf("Thread %d %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		nvht_put(h, k, strlen(k) + 1, v, strlen(v) + 1);
	}
	pthread_exit((void*) 0);
}

void thread_insert() {
	int i;
	pthread_t threads[MAXTHREADNUM];
	void *status;

	nvalloc_init(ID1, 500000000);
	h = nvht_init(ID2);
	long long t1, t2;
	t1 = ustime();
	for (i=0; i<thread_num; ++i) {
		pthread_create(&threads[i], NULL, insert, (void *) i);
	}
	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], &status);
	}
	t2 = ustime();
	printf("%s time diff %lld\n", __func__, t2 - t1);
}

void *hybrid(void *arg) {
	int i;
	int pos = (int) arg;

	int start = TOTALWRITE*pos/thread_num;
	int end = TOTALWRITE*(pos+1)/thread_num;
	printf("Thread %d write %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[30];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		nvht_put(h, k, strlen(k) + 1, v, strlen(v) + 1);
	}

	start = TOTALSEARCH*pos/thread_num;
	end = TOTALSEARCH*(pos+1)/thread_num;
	printf("Thread %d search %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[30];
		char v;
		sprintf(k, KEYSTR, i % TOTALWRITE);
		nvht_get(h, k, strlen(k) + 1, &v);
	}

	pthread_exit((void*) 0);
}

void thread_hybrid() {
	int i;
	pthread_t threads[MAXTHREADNUM];
	void *status;

	nvalloc_init(ID1, 500000000);
	h = nvht_init(ID2);
	long long t1, t2;
	t1 = ustime();
	for (i = 0; i < thread_num; ++i) {
		pthread_create(&threads[i], NULL, hybrid, (void *) i);
	}
	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], &status);
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

/*
 * insert <thread number>
 */
int main(int argc, char *argv[]) {
	if (argc < 3) {
		clean();
		return -1;
	}
	thread_num = atoi(argv[2]);
	if (strcmp(argv[1], "insert") == 0) {
		thread_insert();
	} else if (strcmp(argv[1], "hybrid") == 0) {
		thread_hybrid();
	} else {
		printf("no test for %s\n", argv[1]);
	}
	return 0;
}
