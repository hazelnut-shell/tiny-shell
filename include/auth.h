#ifndef AUTH_H
#define AUTH_H

int exist_user(char * name, char password[]);
int check_auth(char * name, char * passwd);
void add_user(char ** argv);
char * login();

#endif