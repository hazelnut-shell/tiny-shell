#include "history.h"
#include "tsh.h"
#include "helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* where to store the next history record
indicate the position of the oldest command as well (if there is a command at history_idx)*/
int history_idx = 0;        
char history[MAXHISTORY][MAXLINE];  /* the last 10 records of history */


void init_history() {  
    char path[MAXLINE];
    sprintf(path, "./home/%s/.tsh_history", username);

    FILE * fp = fopen(path, "r");
    if(fp == NULL){
        unix_error("fopen");
    }

    int i = 0;
    while(fgets(history[i], MAXLINE, fp) != NULL){
        i++;        
    }

    fclose(fp);

    for(int j = i; j < MAXHISTORY; j++){
        history[j][0] = '\0';
    }

    history_idx = i % MAXHISTORY;
}


int history_start() {
    int start = history_idx;
    if(history[start][0] == '\0'){
        start = 0;
    }
    return start;
}

void add_history(char * cmdline) {
    strcpy(history[history_idx], cmdline);
    history_idx = (history_idx + 1) % MAXHISTORY;

    // change .tsh_history  
    char path[MAXLINE];
    sprintf(path, "./home/%s/.tsh_history", username);

    int start = history_start();

    FILE * fp = fopen(path, "w");
    if(fp == NULL){
        unix_error("fopen");
    }

    for(int count = 0; count < MAXHISTORY && history[start][0]!='\0'; count++){
        fprintf(fp, "%s", history[start]);
        start = (start + 1) % MAXHISTORY;
    }

    fclose(fp);
}

void list_history() {
    int start = history_start();

    for(int count = 0; count < MAXHISTORY && history[start][0] != '\0'; count++){
        printf("%d %s", count + 1, history[start]);
        start = (start + 1) % MAXHISTORY; 
    }
} 

char * nth_history(int n) {
    int start = history_start();
    start = (start + n - 1) % MAXHISTORY;
    return history[start];   // history[start] might be '\0'
}

void exec_nth_cmd(char ** argv) {
    char nstr[MAXLINE];
    char err_msg[50];
    int i;
    for(i=1 ; argv[0][i]!='\0'; i++){   // don't consider the case where there are spaces between ! and the history number for now
        nstr[i-1] = argv[0][i];
    }
    nstr[i-1] = '\0';

    int n = atoi(nstr);
    if(n > MAXHISTORY){
        sprintf(err_msg, "only support the last %d commands\n", MAXHISTORY);
        print_error(err_msg);
        return;
    }

    char * cmdline = nth_history(n);

    if(cmdline[0] == '\0'){
        sprintf(err_msg, "no %dth command yet\n", n);
        print_error(err_msg);
        return;
    }
    eval(cmdline);
}