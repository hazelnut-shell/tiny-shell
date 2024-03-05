#ifndef PROC_H
#define PROC_H

#include <sys/types.h>

void add_proc(char * name, pid_t pid, pid_t ppid, char * stat);
void remove_proc(pid_t pid);
void write_proc(char * name, pid_t pid, pid_t ppid, char * stat);
void change_proc_stat(pid_t pid, char * stat);

#endif