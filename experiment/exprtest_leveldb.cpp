/*
g++ -o exprtest_leveldb exprtest_leveldb.cpp libleveldb.a -lpthread -I ./include
*/
#include <iostream>
#include <sys/time.h>
#include <cstdlib>
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

/*
int main()
{
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;
 
    options.create_if_missing = true;
    //options.error_if_exists = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);
    if (!s.ok()) {
        cerr << s.ToString() << endl;
        return -1;
    }
 
    std::string value;
    s = db->Put(leveldb::WriteOptions(), "k1", "v1");
    cout << s.ok() << endl;
    s = db->Get(leveldb::ReadOptions(), "k1", &value);
    cout << s.ok() << " " << value << std::endl;
    s = db->Delete(leveldb::WriteOptions(), "k1");
    cout << s.ok() << endl;
    value.clear();
    s = db->Get(leveldb::ReadOptions(), "k1", &value);
    cout << s.ok() << "" << value << std::endl;
 
    delete db;
    return 0;
}
*/

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

void test_insert(int count) {
    int i;
    long long t1, t2;

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;

    options.create_if_missing = true;
    options.error_if_exists = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);

    i = 0;
    t1 = ustime();
    while(i++ < count) {
        char k[200];
        char v[200];
        sprintf(k, KEYSTR, i);
        sprintf(v, VALUESTR, i);
        s = db->Put(write_options, k, v);
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));
    delete db;
}

void test_insertr(int count) {
    int i, j;
    long long t1, t2;

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;

    options.create_if_missing = true;
    options.error_if_exists = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);

    i = 0, j=count-1;
    t1 = ustime();
    while(i<j) {
        char k[200];
        char v[200];
        sprintf(k, KEYSTR, i);
        sprintf(v, VALUESTR, i);
        s = db->Put(write_options, k, v);
        if (i>=j) break;
        sprintf(k, KEYSTR, j);
        sprintf(v, VALUESTR, j);
        s = db->Put(write_options, k, v);
        ++i;
        --j;
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));
    delete db;
}

void test_search(int count) {
    int i;
    long long t1, t2;

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;
    // options.block_cache = leveldb::NewLRUCache(100 * 1048576);
    options.compression = leveldb::kNoCompression;
    options.create_if_missing = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);
    i = 0;
    t1 = ustime();
    while(i++ < count) {
        char k[200];
        string v;
        sprintf(k, KEYSTR, i);
        s = db->Get(leveldb::ReadOptions(), k, &v);
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));
    delete db;
    // delete options.block_cache;
}

void test_searchr(int count) {
    int i,j;
    long long t1, t2;

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;

    options.create_if_missing = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);

    i = 0, j=count-1;
    t1 = ustime();
    while(i < j) {
        char k[200];
        string v1, v2;
        sprintf(k, KEYSTR, i);
        s = db->Get(leveldb::ReadOptions(), k, &v1);
        if (i>=j) break;
        sprintf(k, KEYSTR, j);
        s = db->Get(leveldb::ReadOptions(), k, &v2);
        ++i;
        --j;
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));
    delete db;
}

void test_del(int count) {
    int i;
    long long t1, t2;

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;

    options.create_if_missing = true;
    std::string dbpath = "testdb";
    s = leveldb::DB::Open(options, dbpath, &db);

    i = 0;
    t1 = ustime();
    while(i++ < count) {
        char k[200];
        sprintf(k, KEYSTR, i);
        s = db->Delete(write_options, k);
    }
    t2 = ustime();
    printf("%s time diff %lld, qps %f\n", __func__,  t2 - t1, count*1000000.0/(t2-t1));
    delete db;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        return -1;
    }
    int count = atoi(argv[2]);
    printf("count %d\n", count);
    gen_templ(atoi(argv[3]), atoi(argv[4]));
    if (strcmp(argv[1], "insert")==0) {
        test_insert(count);
    } else if (strcmp(argv[1], "insertr")==0) {
        test_insertr(count);
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
	if (KEYSTR != NULL) {
		free(KEYSTR);
	}
	if (VALUESTR != NULL) {
		free(VALUESTR);
	}
    return 0;
}
