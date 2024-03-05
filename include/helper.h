#ifndef HELPER_H
#define HELPER_H

extern char prompt[];

void usage(void);
void unix_error(char * msg);
void app_error(char * msg);
void print_error(char * msg);
void print_prompt(char * prompt);

// string helper function
int number_from_string(const char * st, const char * end);

#endif