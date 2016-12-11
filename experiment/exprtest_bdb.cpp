/*
 * g++ ./exprtest_bdb.cpp -o mytmpfs/exprtest_bdb -ldb
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void test_insert(int count, int type) {
    int i;
    long long t1, t2;

    int ret, ret_c;
    u_int32_t db_flags, env_flags;
    DB *dbp = NULL;
    DB_ENV *envp = NULL;
    DBT key, data;
    DB_TXN *txn = NULL;
    ret = db_env_create(&envp, 0);
    // env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; 
    env_flags = DB_CREATE | DB_INIT_MPOOL; 
    ret = envp->open(envp, db_home_dir, env_flags, 0);
    ret = db_create(&dbp, envp, 0);
    // db_flags = DB_CREATE | DB_AUTO_COMMIT;
    db_flags = DB_CREATE;
    if (type == 2) {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
    } else {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
    }

    i = 0;
    t1 = ustime();
    while(i++ < count) {
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
        // ret = envp->txn_begin(envp, NULL, &txn, 0);
        ret = dbp->put(dbp, txn, &key, &data, 0);
        dbp->sync(dbp, 0);
        // ret = txn->commit(txn, 0);
    }
    t2 = ustime();
    //printf("%s time diff %lld\n", __func__,  t2 - t1);
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));

    if (dbp != NULL) {
        ret_c = dbp->close(dbp, 0);
    }
    if (envp != NULL) {
        ret_c = envp->close(envp, 0);
    }
}

void test_insertr(int count, int type) {
    int i, j;
    long long t1, t2;

    int ret, ret_c;
    u_int32_t db_flags, env_flags;
    DB *dbp = NULL;
    DB_ENV *envp = NULL;
    DBT key, data;
    DB_TXN *txn = NULL;
    ret = db_env_create(&envp, 0);
    // env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; 
    env_flags = DB_CREATE | DB_INIT_MPOOL; 
    ret = envp->open(envp, db_home_dir, env_flags, 0);
    ret = db_create(&dbp, envp, 0);
    // db_flags = DB_CREATE | DB_AUTO_COMMIT;
    db_flags = DB_CREATE;
    if (type == 2) {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);    
    } else {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
    }

    i = 0, j=count-1;
    t1 = ustime();
    while(i<j) {
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
        ret = dbp->put(dbp, txn, &key, &data, 0);
        dbp->sync(dbp, 0);
        if (i>=j) break;

        sprintf(k, KEYSTR, j);
        sprintf(v, VALUESTR, j);
        memset(&key, 0, sizeof(DBT));
        memset(&data, 0, sizeof(DBT));
        key.data = &k;
        key.size = strlen(k) + 1;
        data.data = &v;
        data.size = strlen(v) + 1;
        ret = dbp->put(dbp, txn, &key, &data, 0);
        dbp->sync(dbp, 0);
        ++i;
        --j;
    }
    t2 = ustime();
    //printf("%s time diff %lld\n", __func__,  t2 - t1);
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));

    if (dbp != NULL) {
        ret_c = dbp->close(dbp, 0);
    }
    if (envp != NULL) {
        ret_c = envp->close(envp, 0);
    }
}

void test_search(int count, int type) {
    int i;
    long long t1, t2;

    int ret, ret_c;
    u_int32_t db_flags, env_flags;
    DB *dbp = NULL;
    DB_ENV *envp = NULL;
    DBT key, data;
    DB_TXN *txn = NULL;
    ret = db_env_create(&envp, 0);
    // env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; 
    env_flags = DB_CREATE | DB_INIT_MPOOL; 
    ret = envp->open(envp, db_home_dir, env_flags, 0);
    ret = db_create(&dbp, envp, 0);
    // db_flags = DB_CREATE | DB_AUTO_COMMIT;
    db_flags = DB_RDONLY;
    if (type == 2) {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
    } else {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
    }

    i = 0;
    t1 = ustime();
    while(i++ < count) {
        char k[200];
        char v[200];
        sprintf(k, KEYSTR, i);
        memset(&key, 0, sizeof(DBT));
        memset(&data, 0, sizeof(DBT));
        key.data = &k;
        key.size = strlen(k) + 1;
        // ret = envp->txn_begin(envp, NULL, &txn, 0);
        ret = dbp->get(dbp, txn, &key, &data, 0);
        // ret = txn->commit(txn, 0);
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));

    if (dbp != NULL) {
        ret_c = dbp->close(dbp, 0);
    }
    if (envp != NULL) {
        ret_c = envp->close(envp, 0);
    }
}

void test_searchr(int count, int type) {
    int i, j;
    long long t1, t2;

    int ret, ret_c;
    u_int32_t db_flags, env_flags;
    DB *dbp = NULL;
    DB_ENV *envp = NULL;
    DBT key, data;
    DB_TXN *txn = NULL;
    ret = db_env_create(&envp, 0);
    // env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; 
    env_flags = DB_CREATE | DB_INIT_MPOOL; 
    ret = envp->open(envp, db_home_dir, env_flags, 0);
    ret = db_create(&dbp, envp, 0);
    // db_flags = DB_CREATE | DB_AUTO_COMMIT;
    db_flags = DB_RDONLY;
    if (type == 2) {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
    } else {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
    }

    i = 0, j=count-1;
    t1 = ustime();
    while(i<j) {
        char k[200];
        char v[200];
        sprintf(k, KEYSTR, i);
        memset(&key, 0, sizeof(DBT));
        memset(&data, 0, sizeof(DBT));
        key.data = &k;
        key.size = strlen(k) + 1;
        ret = dbp->get(dbp, txn, &key, &data, 0);
        if (i>=j) break;

        sprintf(k, KEYSTR, j);
        memset(&key, 0, sizeof(DBT));
        memset(&data, 0, sizeof(DBT));
        key.data = &k;
        key.size = strlen(k) + 1;
        ret = dbp->get(dbp, txn, &key, &data, 0);
        ++i;
        --j;
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));

    if (dbp != NULL) {
        ret_c = dbp->close(dbp, 0);
    }
    if (envp != NULL) {
        ret_c = envp->close(envp, 0);
    }
}

void test_del(int count, int type) {
    int i;
    long long t1, t2;

    int ret, ret_c;
    u_int32_t db_flags, env_flags;
    DB *dbp = NULL;
    DB_ENV *envp = NULL;
    DBT key, data;
    DB_TXN *txn = NULL;
    ret = db_env_create(&envp, 0);
    // env_flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; 
    env_flags = DB_CREATE | DB_INIT_MPOOL; 
    ret = envp->open(envp, db_home_dir, env_flags, 0);
    ret = db_create(&dbp, envp, 0);
    // db_flags = DB_CREATE | DB_AUTO_COMMIT;
    db_flags = DB_CREATE;
    if (type == 2) {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_HASH, db_flags, 0);
    } else {
        ret = dbp->open(dbp, NULL, file_name, NULL, DB_BTREE, db_flags, 0);
    }

    i = 0;
    t1 = ustime();
    while(i++ < count) {
        char k[200];
        sprintf(k, KEYSTR, i);
        memset(&key, 0, sizeof(DBT));
        key.data = &k;
        key.size = strlen(k) + 1;
        // ret = envp->txn_begin(envp, NULL, &txn, 0);
        ret = dbp->del(dbp, txn, &key, 0);
        // ret = txn->commit(txn, 0);
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));

    if (dbp != NULL) {
        ret_c = dbp->close(dbp, 0);
    }
    if (envp != NULL) {
        ret_c = envp->close(envp, 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        return -1;
    }
    int count = atoi(argv[2]);
    int type = atoi(argv[3]); /*1 btree 2 linear hash*/
    printf("count %d\n", count);
    gen_templ(atoi(argv[4]), atoi(argv[5]));
    if (strcmp(argv[1], "insert")==0) {
        test_insert(count, type);
    } else if (strcmp(argv[1], "insertr")==0) {
        test_insertr(count, type);
    } else if (strcmp(argv[1], "load")==0) {
        test_insert(count, type);
    } else if (strcmp(argv[1], "search")==0) {
        test_search(count, type);
    } else if (strcmp(argv[1], "searchr")==0) {
        test_searchr(count, type);
    } else if (strcmp(argv[1], "delete")==0) {
        test_del(count, type);
    } else {
        printf("No test for %s\n", argv[1]);
    }
	if (KEYSTR != NULL) {
		free(KEYSTR);
	}
	if (VALUESTR != NULL) {
		free(VALUESTR);
	}
    return 0;
}
