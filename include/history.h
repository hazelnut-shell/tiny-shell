#ifndef HISTORY_H
#define HISTORY_H

#include "tsh.h"

#define MAXHISTORY   10   /* max records of history */ 

extern int history_idx;
extern char history[MAXHISTORY][MAXLINE]; 

void init_history();
int history_start();
void add_history(char * cmdline);
void list_history();
char * nth_history(int n);
void exec_nth_cmd(char ** argv);

#endif