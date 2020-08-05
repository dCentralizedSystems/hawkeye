//
// Created by rsnook on 8/5/20.
//

#ifndef _LOGGER_H
#define _LOGGER_H

void log_init(void);
void log_close(void);
// log to syslog and exit
void log_syslog_panic(const char* fmt, ...);
void log_syslog(const char* fmt, ...);

#endif //_LOGGER_H
