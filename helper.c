#include "helper.h"
#include "tsh.h"
#include <stdio.h>
#include <errno.h>

char prompt[] = "tsh> ";    /* command line prompt */

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine 
 */
void unix_error(char * msg) {
    char str[MAXLINE];
    sprintf(str, "%s: %s\n", msg, strerror(errno));  
    print_error(str);
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char * msg) {
    char str[MAXLINE];
    sprintf(str, "%s\n", msg);
    print_error(msg);
    exit(1);
}

void print_error(char * msg) {
    printf("\033[0;31m");   // set color to red
    printf("%s", msg);    
    printf("\033[0m");      // reset color
}

void print_prompt(char * prompt) {
    printf("\033[0;34m");   // set color to red
    printf("%s", prompt);
    fflush(stdout);    
    printf("\033[0m");      // reset color
}

// string helper functions

// extract a number from string [st, end)
int number_from_string(const char * st, const char * end) {
    
    char str[10];
    const char * p = st;

    int i = 0;
    for(; p + i != end; i++) {
        str[i] = p[i];

    }
    str[i] = '\0';
    return atoi(str);
}