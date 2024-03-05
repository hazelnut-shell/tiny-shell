#include "proc.h"
#include "tsh.h"
#include "job.h"
#include "helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

void write_proc(char * name, pid_t pid, pid_t ppid, char * stat) {
    char path[MAXLINE];
    sprintf(path, "./proc/%d/status", pid);
    FILE * fp = fopen(path, "w");
    if(fp == NULL){
        unix_error("fopen");
    }

    fprintf(fp, "Name: %s\n", name);
    
    fprintf(fp, "Pid: %d\n", pid);
    fprintf(fp, "PPid: %d\n", ppid);
    fprintf(fp, "PGid: %d\n", pid);
    fprintf(fp, "Sid: %d\n", shell_pid);

    fprintf(fp, "STAT: %s\n", stat);

    fprintf(fp, "Username: %s\n", username);
    fclose(fp);
}

void change_proc_stat(pid_t pid, char * stat){
    char path[MAXLINE];
    sprintf(path, "./proc/%d/status", pid);

    // check if the file exists (some processes in the process group may have terminated and their proc files have been removed)
    if(access(path, F_OK) != 0) {
        return;
    }

    FILE * fp = fopen(path, "r+");
    if(fp == NULL){
        unix_error("fopen");
    }

    char str[MAXLINE];

    for(int cnt = 0; cnt < 5; cnt++){
        fgets(str, MAXLINE, fp);
    }
    fprintf(fp, "STAT: %s\n", stat);
    fprintf(fp, "Username: %s\n", username);
    
    fclose(fp);
}

void add_proc(char * name, pid_t pid, pid_t ppid, char * stat){
    char path[MAXLINE];
    sprintf(path, "./proc/%d", pid);
    if(mkdir(path, 0777)!=0){
        unix_error("mkdir error");
    }

    write_proc(name, pid, ppid, stat);
}

void remove_proc(pid_t pid){
    char path[MAXLINE];
    sprintf(path, "./proc/%d/status", pid);
    if(remove(path) != 0){
        unix_error("remove error");
    }

    sprintf(path, "./proc/%d", pid);
    if(rmdir(path)!=0){ // remove directory only if it's empty
        unix_error("rmdir error");
    }
}