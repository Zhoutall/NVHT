#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/time.h>
#include "db.h"

long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

#define MAXTHREADNUM 8
#define TOTAL (1000000)
#define TOTALWRITE (300000)
#define TOTALSEARCH (TOTAL-TOTALWRITE)

const char *db_home_dir = "./envdir";
const char *file_name = "testdb.db";
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

pthread_spinlock_t m;
int thread_num;
int engine_type;
DB *dbp = NULL;
DB_ENV *envp = NULL;

void *insert(void *arg) {
	int i, ret;
	DBT key, data;
	long pos = (long) arg;

	int start = TOTAL*pos/thread_num;
	int end = TOTAL*(pos+1)/thread_num;
	printf("Thread %ld %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[200];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		memset(&key, 0, sizeof(DBT));
		memset(&data, 0, sizeof(DBT));
		key.data = &k;
		key.size = strlen(k) + 1;
		data.data = &v;
		data.size = strlen(v) + 1;
		pthread_spin_lock(&m);
retry:
		ret = dbp->put(dbp, NULL, &key, &data, 0);
		if (ret == DB_LOCK_DEADLOCK) {
			goto retry;
		}
		dbp->sync(dbp, 0);
		pthread_spin_unlock(&m);
	}
	pthread_exit((void*) 0);
}

void thread_insert() {
	int i, ret;
	pthread_t threads[MAXTHREADNUM];
	void *status;
	u_int32_t db_flags, env_flags;
	ret = db_env_create(&envp, 0);
	// env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL;
	env_flags = DB_CREATE | DB_INIT_MPOOL | DB_THREAD;
	ret = envp->open(envp, db_home_dir, env_flags, 0);
	ret = db_create(&dbp, envp, 0);
	// db_flags = DB_CREATE | DB_AUTO_COMMIT;
	db_flags = DB_CREATE | DB_THREAD | DB_DBT_MALLOC;
	if (engine_type == 2) {
		ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
	} else {
		ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
	}
	pthread_spin_init(&m, PTHREAD_PROCESS_PRIVATE);
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
	pthread_spin_destroy(&m);
	if (dbp != NULL) {
		ret = dbp->close(dbp, 0);
	}
	if (envp != NULL) {
		ret = envp->close(envp, 0);
	}
}

void *hybrid(void *arg) {
	int i, ret;
	DBT key, data;
	long pos = (long) arg;

	int start = TOTALWRITE*pos/thread_num;
	int end = TOTALWRITE*(pos+1)/thread_num;
	printf("Thread %ld write %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[200];
		char v[200];
		sprintf(k, KEYSTR, i);
		sprintf(v, VALUESTR, i);
		memset(&key, 0, sizeof(DBT));
		memset(&data, 0, sizeof(DBT));
		key.data = &k;
		key.size = strlen(k) + 1;
		data.data = &v;
		data.size = strlen(v) + 1;
		pthread_spin_lock(&m);
retry:
		ret = dbp->put(dbp, NULL, &key, &data, 0);
		if (ret == DB_LOCK_DEADLOCK) {
			goto retry;
		}
		dbp->sync(dbp, 0);
		pthread_spin_unlock(&m);
	}

	start = TOTALSEARCH * pos / thread_num;
	end = TOTALSEARCH * (pos + 1) / thread_num;
	printf("Thread %ld search %d-%d\n", pos, start, end);
	i = start;
	while (i++ < end) {
		char k[200];
		char v[200];
		sprintf(k, KEYSTR, i % TOTALWRITE);
		memset(&key, 0, sizeof(DBT));
		memset(&data, 0, sizeof(DBT));
		key.data = &k;
		key.size = strlen(k) + 1;
		data.flags |= DB_DBT_MALLOC;
		pthread_spin_lock(&m);
		ret = dbp->get(dbp, NULL, &key, &data, 0);
		pthread_spin_unlock(&m);
	}
	pthread_exit((void*) 0);
}

void thread_hybrid() {
	int i, ret;
	pthread_t threads[MAXTHREADNUM];
	void *status;
	u_int32_t db_flags, env_flags;
	ret = db_env_create(&envp, 0);
	// env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL;
	env_flags = DB_CREATE | DB_INIT_MPOOL | DB_THREAD;
	ret = envp->open(envp, db_home_dir, env_flags, 0);
	ret = db_create(&dbp, envp, 0);
	// db_flags = DB_CREATE | DB_AUTO_COMMIT;
	db_flags = DB_CREATE | DB_THREAD | DB_DBT_MALLOC;
	if (engine_type == 2) {
		ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
	} else {
		ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
	}
	pthread_spin_init(&m, PTHREAD_PROCESS_PRIVATE);
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
	pthread_spin_destroy(&m);
	if (dbp != NULL) {
		ret = dbp->close(dbp, 0);
	}
	if (envp != NULL) {
		ret = envp->close(envp, 0);
	}
}

/*
 * insert <thread number>
 */
int main(int argc, char *argv[]) {
	if (argc < 6) {
		return -1;
	}
	thread_num = atoi(argv[2]);
	engine_type = atoi(argv[3]); /*1 btree 2 linear hash*/
	gen_templ(atoi(argv[4]), atoi(argv[5]));
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
