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

#define MAXTHREADNUM 8
int TOTAL = 1000000;
int TOTALWRITE = 300000;
int TOTALSEARCH = 700000;

//#define KEYSTR "test-key-[%d]"
//#define VALUESTR "test-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-valuetest-value-[%d]"

char *KEYSTR = NULL;
char *VALUESTR = NULL;

void gen_templ(int keylen, int valuelen) {
	KEYSTR = (char *)malloc(keylen * sizeof(char));
	VALUESTR = (char *)malloc(valuelen * sizeof(char));
	int i;
	for (i=4; i<keylen; ++i) {
		KEYSTR[i] = i%26 + 'a';
	}
	for (i=4; i<valuelen; ++i) {
		VALUESTR[i] = i%26 + 'a';
	}
	memcpy(KEYSTR, "[%d]", 4);
	memcpy(VALUESTR, "[%d]", 4);
}

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
		char k[200];
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
	//printf("%s time diff %lld\n", __func__, t2 - t1);
	printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, TOTAL*1000000.0/(t2-t1));
}

void *hybrid(void *arg) {
	int i;
	int pos = (int) arg;

	int start = TOTALWRITE*pos/thread_num;
	int end = TOTALWRITE*(pos+1)/thread_num;
	printf("Thread %d write %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[200];
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
		char k[200];
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
	//printf("%s time diff %lld\n", __func__, t2 - t1);
	printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, TOTAL*1000000.0/(t2-t1));
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
	if (argc < 6) {
		// threadnum keylen valuelen write_percent
		clean();
		return -1;
	}
	thread_num = atoi(argv[2]);
	gen_templ(atoi(argv[3]), atoi(argv[4]));
	int percent = atoi(argv[5]);
	TOTALWRITE = TOTAL * percent / 100;
	TOTALSEARCH = TOTAL-TOTALWRITE;
	if (strcmp(argv[1], "insert") == 0) {
		thread_insert();
	} else if (strcmp(argv[1], "hybrid") == 0) {
		thread_hybrid();
	} else {
		printf("no test for %s\n", argv[1]);
	}
	if (KEYSTR != NULL) {
		free(KEYSTR);
	}
	if (VALUESTR != NULL) {
		free(VALUESTR);
	}
	return 0;
}
