//
// Created by rsnook on 8/5/20.
//
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>

//init flag
static bool b_initialized = false;

void log_init(void) {
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog("hawkeye", LOG_PID | LOG_NDELAY, LOG_USER);
    b_initialized = true;
}

void log_close(void) {
    if (b_initialized) {
        closelog();
    }
}

void log_syslog_panic(const char* fmt, ...) {
    if (b_initialized) {
        va_list args;
        va_start(args, fmt);
        vsyslog(LOG_ERR, fmt, args);
        va_end(args);
    }
    exit(EXIT_FAILURE);
}

void log_syslog(const char* fmt, ...) {
    if (b_initialized) {
        va_list args;
        va_start(args, fmt);
        vsyslog(LOG_INFO, fmt, args);
        va_end(args);
    }
}


