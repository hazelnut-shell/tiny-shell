#include "auth.h"
#include "tsh.h"
#include "helper.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

// if name exists in file passwd, set variable password to be the corresponding password
int exist_user(char * name, char password[]) { 
    char line[MAXLINE * 2];
    char * line_name, * line_password;

    FILE * fp = fopen("./etc/passwd", "r");
    if(fp == NULL){
        unix_error("fopen");
    }

    while(fgets(line, MAXLINE * 2, fp)!=NULL){
        line_name = strtok(line, ":");
        if(strcmp(name, line_name) == 0){
            line_password = strtok(NULL, ":"); 
            strcpy(password, line_password); 
            fclose(fp);
            return 1;
        }
        
    }

    fclose(fp);
    return 0;
}

int check_auth(char * name, char * passwd) {
    char line_password[MAXLINE];
    if(exist_user(name, line_password)){
        if(strcmp(line_password, passwd) == 0){
            return 1;
        }
    }

    return 0;
}

/*
 * login - Performs user authentication for the shell
 *
 * See specificaiton for how this function should act
 *
 * This function returns a string of the username that is logged in
 */
char * login() {
    char * name = (char*)malloc(sizeof(char) * MAXLINE);
    char passwd[MAXLINE];

    while(1){
        printf("username: ");
        fflush(stdout);
        fgets(name, MAXLINE, stdin);    // use fgets, instead of scanf, in case there are spaces in the input
        name[strlen(name) - 1] = '\0';  // deal with '\n' at the end
        if(strcmp(name, "quit")==0){
            free(name);
            exit(0);
        }

        printf("password: ");
        fflush(stdout);
        fgets(passwd, MAXLINE, stdin); 
        passwd[strlen(passwd) - 1] = '\0';
        if(strcmp(passwd, "quit")==0){
            free(name);
            exit(0);
        }
        

        if(check_auth(name, passwd)){
            return name;
        }else{
            print_error("User Authentication failed. Please try again.\n");
        }
    }

}


void add_user(char ** argv) {
    if(strcmp(username, "root")!=0){
        print_error("root privileges required to run adduser.\n");
        return;
    }
    if(argv[1] == NULL  || argv[2]==NULL){
        print_error("need more arguments\n");
        return;
    } else if(argv[3] != NULL){
        print_error("too many arguments\n");
        return;
    }

    char password[MAXLINE];
    if(exist_user(argv[1], password)){
        print_error("User already exists\n");
        return;
    }

    FILE * fp = fopen("./etc/passwd", "a");
    if(fp == NULL){
        unix_error("fopen");
    }
    fprintf(fp, "%s:%s:/home/%s\n", argv[1], argv[2], argv[1]);
    fclose(fp);

    char path[MAXLINE+20];
    sprintf(path, "./home/%s", argv[1]); 

    if(mkdir(path, 0777)!=0)
        unix_error("mkdir error");

    strcat(path, "/.tsh_history");
    FILE * historyp = fopen(path, "w"); 
    if(historyp == NULL){
        unix_error("fopen");
    }
    fclose(historyp); 
}