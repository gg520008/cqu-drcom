#ifndef UTILITY_H
#define UTILITY_H

#ifdef _WIN32

unsigned int sleep(unsigned int second);

#else

#include <unistd.h>

#endif

void println_hex(char *data, int length);

void error(char *message);

#define BUF_SIZE 1000

typedef char str[BUF_SIZE];


#endif
