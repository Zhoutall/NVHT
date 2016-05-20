#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "nvsim.h"

#define PATHNAME "/tmp"
#define PAGE_SIZE 4096

void *nv_get(int64_t nvid, int size) {
	if (size < PAGE_SIZE) {
		size = PAGE_SIZE;
	}
	key_t k = ftok(PATHNAME, nvid);
	if (k==-1) {
		perror("ftok");
		exit(EXIT_FAILURE);
	}
	int shmid = shmget(k, size, 0777 | IPC_CREAT | IPC_EXCL);
	if (shmid == -1) {
//		printf("%s nvid %d\n", __func__, nvid);
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	void *vaddr = shmat(shmid, 0, 0);
	if (vaddr == (void *) (-1)) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	return vaddr;
}

/*
 * TODO: if attached, should return the previous address
 */
void *nv_map(int nvid) {
	key_t k = ftok(PATHNAME, nvid);
	if (k == -1) {
		perror("ftok");
		exit(EXIT_FAILURE);
	}
	int shmid = shmget(k, 0, 0);
	if (shmid == -1) {
//		printf("%s nvid %d\n", __func__, nvid);
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	void *vaddr = shmat(shmid, 0, 0);
	if (vaddr == (void *)(-1)) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	return vaddr;
}

int nv_detach(void *nvaddr) {
	int ret = shmdt(nvaddr);
	if (ret == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int nv_remove(int nvid) {
	key_t k = ftok(PATHNAME, nvid);
	if (k==-1) {
		perror("ftok");
		exit(EXIT_FAILURE);
	}
	int shmid = shmget(k, 0, 0);
	if (shmid == -1) {
		// has been removed
		return 0;
	}
	int ret = shmctl(shmid, IPC_RMID, 0);
	if (ret == -1) {
		perror("shmctl");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int nv_exist(int nvid) {
	key_t k = ftok(PATHNAME, nvid);
	if (k == -1) {
		perror("ftok");
		exit(EXIT_FAILURE);
	}
	int shmid = shmget(k, 0, 0);
	return shmid;
}
