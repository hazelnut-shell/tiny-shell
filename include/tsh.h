#ifndef TSH_H
#define TSH_H

#include <stdbool.h>
#include <sys/types.h>

#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXPIPES     16   /* max pipes per command line */
#define UNUSEDFD     36   /* fd number that is guaranteed not to be used */  

extern int verbose;
extern int shell_pid;
extern char * username;

struct cmd_t {
    char * argv[MAXARGS];
    char * redirection_str[MAXARGS];
    bool is_builtin;
};

void eval(char * cmdline);
int parseline(const char * cmdline, struct cmd_t * cmd, int * cmd_num, int * process_num, bool * should_add_history); 
void set_is_builtin(struct cmd_t * cmd);
void collect_redir_str(struct cmd_t * cmd);

void setup_pipe_and_redir(int cmd_num, int cmd_idx, struct cmd_t * cmd, int pipes[][2]);
void setup_redir(struct cmd_t * cmd);
void save_fd();
void restore_fd();
void close_all_pipes(int pipes[][2], int num);

void exec_builtin_cmd(char ** argv);
void do_bgfg(char ** argv);
void waitfg(pid_t pid);
void do_quit();

typedef void handler_t(int);  
handler_t * Signal(int signum, handler_t * handler);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);

#endif