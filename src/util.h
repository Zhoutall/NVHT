#ifndef __UTIL_H__
#define __UTIL_H__
/*
 * util functions
 */

static unsigned long crc32(const unsigned char *s, unsigned int len);
unsigned int hash_string(char* keystring, int len);
unsigned int hash_integer(unsigned long key);

int random_nvid();

#endif
