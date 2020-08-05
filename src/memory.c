#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>

#include "logger.h"

// Memory allocator wrappers

char* __real_strdup(const char *s);
void* __real_malloc(size_t size);
void* __real_realloc(void *ptr, size_t size);
void* __real_calloc(size_t num, size_t size);

char* __wrap_strdup(const char *s) {
	char *ptr = __real_strdup(s);
	char error[512];
	if (ptr == NULL) {
		strerror_r(errno, (char *) &error, sizeof(error));
		log_syslog("strdup() failed: (%d) %s", errno, error);
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void* __wrap_malloc(size_t size) {
	void *tmp = __real_malloc(size);
	char error[512];
	if (tmp == NULL) {
		strerror_r(errno, (char *) &error, sizeof(error));
		log_syslog("malloc() failed: (%d) %s", errno, error);
		exit(EXIT_FAILURE);
	}
	return tmp;
}

void* __wrap_realloc(void *ptr, size_t size) {
	void *tmp = __real_realloc(ptr, size);
	char error[512];
	if (tmp == NULL) {
		strerror_r(errno, (char *) &error, sizeof(error));
		log_syslog("realloc() failed: (%d) %s", errno, error);
		exit(EXIT_FAILURE);
	}
	return tmp;
}

void* __wrap_calloc(size_t num, size_t size) {
	void *tmp = __real_calloc(num, size);
	char error[512];
	if (tmp == NULL) {
		strerror_r(errno, (char *) &error, sizeof(error));
		log_syslog("calloc() failed: (%d) %s", errno, error);
		exit(EXIT_FAILURE);
	}
	return tmp;
}

