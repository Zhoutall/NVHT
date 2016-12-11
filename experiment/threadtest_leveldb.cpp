#include <iostream>
#include <sys/time.h>
#include <cstdlib>
#include <pthread.h>
#include "leveldb/db.h"
#include "leveldb/cache.h"
using namespace std;

long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

#define MAXTHREADNUM 20
int TOTAL = 1000000;
int TOTALWRITE = 300000;
int TOTALSEARCH = 700000;

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

//pthread_mutex_t m;
int thread_num;
leveldb::DB* db;

void *insert(void *arg) {
	int i;
	long pos = (long) arg;

	int start = TOTAL*pos/thread_num;
	int end = TOTAL*(pos+1)/thread_num;
	printf("Thread %ld %d-%d\n", pos, start, end);
	leveldb::Status s;
	leveldb::WriteOptions write_options;
	write_options.sync = true;
	i = start;
	while (i++ < end) {
		char k[200];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
//		pthread_mutex_lock(&m);
		s = db->Put(write_options, k, v);
//		pthread_mutex_unlock(&m);
	}
	pthread_exit((void*) 0);
}

void thread_insert() {
	int i;
	pthread_t threads[MAXTHREADNUM];
	void *status;

	leveldb::Options options;
	leveldb::Status s;
	options.create_if_missing = true;
	options.error_if_exists = true;
	std::string dbpath = "testdb";
	s = leveldb::DB::Open(options, dbpath, &db);

//	pthread_mutex_init(&m, NULL);
	long long t1, t2;
	t1 = ustime();
	for (i=0; i<thread_num; ++i) {
		pthread_create(&threads[i], NULL, insert, (void *) i);
	}
	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], &status);
	}
	t2 = ustime();
	printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, TOTAL*1000000.0/(t2-t1));
//	pthread_mutex_destroy(&m);
	delete db;
}

void *hybrid(void *arg) {
	int i;
	long pos = (long) arg;

	int start = TOTALWRITE*pos/thread_num;
	int end = TOTALWRITE*(pos+1)/thread_num;
	printf("Thread %d write %d-%d\n", pos, start, end);
	leveldb::Status s;
	leveldb::WriteOptions write_options;
	write_options.sync = true;
	i = start;
	while (i++ < end) {
		char k[200];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		s = db->Put(write_options, k, v);
	}

	start = TOTALSEARCH*pos/thread_num;
	end = TOTALSEARCH*(pos+1)/thread_num;
	printf("Thread %d search %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[200];
		string v;
		sprintf(k, KEYSTR, i % TOTALWRITE);
		s = db->Get(leveldb::ReadOptions(), k, &v);
	}
	pthread_exit((void*) 0);
}

void thread_hybrid() {
	int i;
	pthread_t threads[MAXTHREADNUM];
	void *status;

	leveldb::Options options;
	leveldb::Status s;
	options.create_if_missing = true;
	options.error_if_exists = true;
	std::string dbpath = "testdb";
	s = leveldb::DB::Open(options, dbpath, &db);

//	pthread_mutex_init(&m, NULL);
	long long t1, t2;
	t1 = ustime();
	for (i=0; i<thread_num; ++i) {
		pthread_create(&threads[i], NULL, hybrid, (void *) i);
	}
	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], &status);
	}
	t2 = ustime();
	printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, TOTAL*1000000.0/(t2-t1));
//	pthread_mutex_destroy(&m);
	delete db;
}

/*
 * insert <thread number>
 */
int main(int argc, char *argv[]) {
	if (argc < 6) {
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
