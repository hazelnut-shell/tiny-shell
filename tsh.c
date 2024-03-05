/* 
 * tsh - A tiny shell program with job control
 * 
 * test user:
 *  username: root
 *  password: pass
 */
#define _POSIX_C_SOURCE 200809L

#include "job.h"
#include "tsh.h"
#include "proc.h"
#include "history.h"
#include "helper.h"
#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>

extern char ** environ;     /* defined in libc */
int verbose = 0;            /* if true, print additional output */  
char sbuf[MAXLINE];         /* for composing sprintf messages */ 
char * username;            /* The name of the user currently logged into the shell */
int shell_pid;

/*
 * main - The shell's main routine 
 */
int main(int argc, char ** argv) {
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2); 

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	    default:
            usage();
	    }
    }

    /* Install the signal handlers */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);  

    /* Initialize the job list */
    initjobs(jobs);

    /* Have a user log into the shell */
    username = login();

    shell_pid = getpid();
    add_proc("tsh", shell_pid, getppid(), "Rs+");
    
    /* Init history for the user that has logged in */
    init_history();
    
    /* Execute the shell's read/eval loop */
    while (1) {
        /* Read command line */
        if (emit_prompt) {
            print_prompt(prompt);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) 
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char * cmdline) {   
    // char * argv[MAXARGS];
    int bg;
    pid_t pid;

    int cmd_num, process_num;
    bool should_add_history;
    struct cmd_t cmd[MAXPIPES];
    memset(cmd, 0, sizeof(cmd));

    bg = parseline(cmdline, cmd, &cmd_num, &process_num, &should_add_history);
    if(cmd_num == 0){
        return;
    }

    bool all_builtin = (process_num == 0);

    // if any command in the pipeline contains '!', we don't add this command line to history
    if(should_add_history)
        add_history(cmdline);

    
    // create pipes
    int pipes[MAXPIPES][2];
    for(int i = 0; i < cmd_num - 1; i++) {
        if(pipe(pipes[i])) {
            unix_error("creating pipes failed");
        }
    }

    pid_t pgid = 0;
    pid_t child_pid[MAXPIPES] = {0};
    int child_idx = 0;

    sigset_t mask_all, prev;
    int state;
    char stat[3];

    if(!all_builtin) {
        // block all signals  
        // to ensure that we handle signals such as SIGINT, SIGCHLD, etc. after the job is added to jobs list and relevant proc files are created
        sigfillset(&mask_all);
        sigprocmask(SIG_BLOCK, &mask_all, &prev); 

        if(!bg) {
            state = FG;
            strcpy(stat, "R+");
            change_proc_stat(shell_pid, "Ss");
        } else {
            state = BG;
            strcpy(stat, "R");
        }
    }

    for(int i = 0; i < cmd_num; i++) {
        if(cmd[i].is_builtin) {
            save_fd();
            setup_pipe_and_redir(cmd_num, i, &cmd[i], pipes);

            exec_builtin_cmd(cmd[i].argv);

            restore_fd();
            continue;
        } 

        pid_t pid = fork();
        if(pid > 0) {
            if(pgid == 0) {
                pgid = pid;
            }
            child_pid[child_idx++] = pid;
            add_proc(cmd[i].argv[0], pid, shell_pid, stat); 
        }

        if(pid == 0) {
            // unblock in child. Otherwise, child could not deal with blocked signals
            sigprocmask(SIG_SETMASK, &prev, NULL);  

            setup_pipe_and_redir(cmd_num, i, &cmd[i], pipes);

            // child close all pipes
            close_all_pipes(pipes, cmd_num - 1);

            setpgid(0, pgid);

            if(execve(cmd[i].argv[0], cmd[i].argv, environ) < 0){
                char msg[MAXLINE + 30];
                sprintf(msg, "%s: Command not found.\n", cmd[i].argv[0]);
                print_error(msg);
                exit(1);
            }
        }
    } 

    // shell process close all pipes
    close_all_pipes(pipes, cmd_num - 1);


    if(!all_builtin) {
        addjob(jobs, pgid, process_num, state, cmdline, child_pid);

        sigprocmask(SIG_SETMASK, &prev, NULL);  // unblock

        if(!bg) {
            waitfg(pgid); 
            change_proc_stat(shell_pid, "Rs+");
        }
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* parsing-related functions start */ 

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char * cmdline, struct cmd_t * cmd, int * cmd_num, int * process_num, bool * should_add_history) {
    static char array[MAXLINE];  /* holds local copy of command line */
    char * buf = array;          /* ptr that traverses command line */
    char * delim;                /* points to first space delimiter */
    int argc;                    /* number of args */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space, so that the last argv can be added to argv list just like other argvs */
    
    *cmd_num = 0;   
    *process_num = 0;
    *should_add_history = true;

    argc = 0;

    /* Build the argv list */

    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;
        
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    } else if(*buf == '"') {
        buf++;
        delim = strchr(buf, '"');
    } else {
        delim = strchr(buf, ' ');
    }    

    while (delim) {
	    *delim = '\0';

        if(strcmp(buf, "|") == 0) {
            cmd[*cmd_num].argv[argc] = NULL;

            // reset state for next command
            argc = 0;
            (*cmd_num)++;
        } else {
            cmd[*cmd_num].argv[argc++] = buf;
        }

	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	    if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        } else if(*buf == '"') {
            buf++;
            delim = strchr(buf, '"');
        } else {
            delim = strchr(buf, ' ');
        }
    }
    cmd[*cmd_num].argv[argc] = NULL;
    (*cmd_num)++;

    /* ignore blank line, and incomplete pipeline, e.g., echo abc | grep a | */
    if (argc == 0) {
        *cmd_num = 0;
        return 1;
    } 

    int bg = 0;
    /* should the job run in the background?
    if the last argv of the last command is '&', yes
    */
    char * last_argv = cmd[(*cmd_num) - 1].argv[argc - 1];
    if(strcmp(last_argv, "&") == 0) { // if bg, remove the last & from argv list
        if(argc == 1) { // [command] | [command] | &
            *cmd_num = 0;
            return 1;
        }

        // background job
        cmd[(*cmd_num) - 1].argv[argc - 1] = NULL;
        argc--;
        bg = 1;
    }

    // collect redirection string (set argv and redirection_str properties of cmd_t),
    // and set is_builtin property of cmd_t
    // count process_num, set should_add_history
    for(int i = 0; i < (*cmd_num); i++) {
        collect_redir_str(&cmd[i]);
        set_is_builtin(&cmd[i]);
        if(!cmd[i].is_builtin) {
            (*process_num)++;
        }

        if(cmd[i].is_builtin) {
            if(cmd[i].argv[0][0] == '!') {
                *should_add_history = false;
            }
        }
    }

    return bg;
}

/*
redirection operation general format (based on POSIX shell standards):
[n]redir-op file

n is an optional fd. If n is provided, no space allowed between n and redir-op.
redir-op is '>' or '<'
file can be filename or &<fd> 

0 or more spaces between redir-op and file is allowed

- currently supporting examples: 
echo abc>test.txt × currently not supported. 'abc' is not part of the redirecting operation, and it must be separated by spaces from '>' 
echo "123">test.txt √ 
echo 123 >test.txt √ 
echo 123>test.txt 123, the fist '123' will be taken as a fd in the shell execution environment.
*/ 

/** This function strips off all redirection operations from a command, 
 * and collects them into the redirection_str field in struct cmd_t. 
 * Each redirection operation as a whole can occur anywhere in a command, for example: 2> err.txt /bin/echo >data.txt -e "hello\nworld"
 * */ 
void collect_redir_str(struct cmd_t * cmd) {
    // use argv_idx to traverse the original argv array, 
    int argv_idx = 0;
    int new_argv_idx = 0;    // new_argv_idx, to construct the real command part (without redirection operations), always slower than argv_idx
    int redir_idx = 0;

    // a pointer to the cmd->argv array
    // so in the following code we don't need to write cmd->argv every time
    char ** argv = cmd->argv;
    char ** redirection_str = cmd->redirection_str;

    for(; argv[argv_idx] != NULL; ) {
        char * op_ptr = strchr(argv[argv_idx], '>');
        if(op_ptr == NULL) {
            op_ptr = strchr(argv[argv_idx], '<');
        } 

        // doesn't contain '<' or '>' 
        if(op_ptr == NULL) {
            argv[new_argv_idx++] = argv[argv_idx];
            argv_idx++;
            continue;
        }

        // argv[i] contains '<' or '>' 
        redirection_str[redir_idx++] = argv[argv_idx];
        int len = strlen(argv[argv_idx]);

        if(argv[argv_idx][len-1] == '>' || argv[argv_idx][len-1] == '<' ) { // there exist spaces between op and file
            argv_idx++;
            redirection_str[redir_idx++] = argv[argv_idx];
        }
        argv_idx++;
    }
    
    // end argv and redirection_str array
    argv[new_argv_idx] = NULL;
    redirection_str[redir_idx] = NULL;
}

// set the is_builtin property of cmd
void set_is_builtin(struct cmd_t * cmd) {
    char ** argv = cmd->argv;

    bool is_builtin = false;
    
    if(strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "fg")==0 ){
       is_builtin = true;
    }else if(strcmp(argv[0], "jobs") == 0){
       is_builtin = true;
    }else if(strcmp(argv[0], "adduser") == 0){ 
        is_builtin = true;
    }else if (strcmp(argv[0], "history") == 0){
        is_builtin = true;
    }else if (argv[0][0] == '!'){
        is_builtin = true;
    }else if (strcmp(argv[0], "logout") == 0){
        is_builtin = true;
    }else if(strcmp(argv[0], "quit") == 0){
        is_builtin = true;
    }

    cmd->is_builtin = is_builtin;
}
/* parsing-related functions end */ 

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* fd operations start */ 

// | < >
void setup_pipe_and_redir(int cmd_num, int cmd_idx, struct cmd_t * cmd, int pipes[][2]) {

    // set up pipe redir |
    if(cmd_idx < cmd_num - 1) { // not the last command in the line, output to pipe
        dup2(pipes[cmd_idx][1], STDOUT_FILENO);
    }

    if(cmd_idx - 1 >= 0) {
        dup2(pipes[cmd_idx-1][0], STDIN_FILENO);
    }   

    // < >
    setup_redir(cmd); 
}

// < >
void setup_redir(struct cmd_t * cmd) {
    char ** redir_str = cmd -> redirection_str;

    for(int i = 0; redir_str[i] != NULL; i++) {
        int old_fd, new_fd;
        char * old_fd_str;
        char * op_pos = redir_str[i];
        
        // determine new fd
        if(redir_str[i][0] == '>' ) {
            new_fd = STDOUT_FILENO;
        } else if(redir_str[i][0] == '<') {
            new_fd = STDIN_FILENO;
        } else {
            op_pos = strchr(redir_str[i], '<');
            if(op_pos == NULL) {
                op_pos = strchr(redir_str[i], '>');
            }

            new_fd = number_from_string(redir_str[i], op_pos);
        }

        if( *(op_pos + 1) == '\0') {    // old fd is in the next entry
            old_fd_str = redir_str[i + 1];
            i++;
        } else { // old fd is in this entry
            old_fd_str = op_pos + 1;
        }

        // determine old fd: &[fd] or <filename>
        if((*old_fd_str) == '&') {
            // the number after & should represent an already open file 
            old_fd = number_from_string(old_fd_str + 1, old_fd_str + 1 + strlen(old_fd_str + 1));
        } else {
            // open file
            if((*op_pos) == '>') {
                // if the file already exists, discard its content and treat it as an empty file
                old_fd = open(old_fd_str, O_WRONLY | O_CREAT | O_TRUNC, 0777);     
            } else {
                old_fd = open(old_fd_str, O_RDONLY, 0777);
            }

            if(old_fd == -1) {
                unix_error("fopen");
            }
        }

        // set up redirection
        dup2(old_fd, new_fd);

        // we can close the newly opened file after its descriptor entry has been copied to new_fd
        if((*old_fd_str) != '&') {
            close(old_fd);
        }
    }
}

void save_fd() {
    // save copies of original stdin, stdout, stderr 
    // (currently, our shell doesn't support other fds in the shell execution environment, only these 3 fds are in the shell execution environment.)
    // because bulit-in commands execute in the shell process, we need to restore fds later.
    dup2(STDIN_FILENO, UNUSEDFD);
    dup2(STDOUT_FILENO, UNUSEDFD + 1);
    dup2(STDERR_FILENO, UNUSEDFD + 2);
}

void restore_fd() {
    // restore stdin, stdout, stderr, and delete backup copies
    dup2(UNUSEDFD, STDIN_FILENO);
    dup2(UNUSEDFD + 1, STDOUT_FILENO);
    dup2(UNUSEDFD + 2, STDERR_FILENO);
    close(UNUSEDFD);
    close(UNUSEDFD + 1);
    close(UNUSEDFD + 2);

}

void close_all_pipes(int pipes[][2], int num) {
    for(int pipe_idx = 0; pipe_idx < num; pipe_idx++) {
        close(pipes[pipe_idx][0]);                
        close(pipes[pipe_idx][1]);
    }
}
/* fd operations end */ 

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* functions related to builtin commands execution start */

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately in the shell process itself.  
 */
void exec_builtin_cmd(char ** argv) {
    if(strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "fg")==0 ){
        do_bgfg(argv);
    }else if(strcmp(argv[0], "jobs") == 0){
        if(argv[1] == NULL)
            listjobs(jobs);
        else
            print_error("too many arguments\n");
    }else if(strcmp(argv[0], "adduser")==0){ 
        add_user(argv);
    }else if (strcmp(argv[0], "history") == 0){
        if(argv[1] != NULL){
            print_error("too many arguments\n");   
        }
        list_history();
    }else if (argv[0][0] == '!' ){
        exec_nth_cmd(argv);
    }else if (strcmp(argv[0], "logout") ==0 ){
        if(check_suspend()){
            print_error("There are suspended jobs.\n");
        }else{
            do_quit();
        }
    }else if(strcmp(argv[0], "quit") == 0){
        do_quit();
    }
}

void do_quit() {
    free(username);

    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    for(int i = 0; i < MAXJOBS; i++){
	    if (jobs[i].state != UNDEF) {
            kill(-jobs[i].pgid, SIGKILL);
        }
    }

    pid_t pgid = check_exists_job();
    
    while(pgid != 0){
        sigsuspend(&prev);
        pgid = check_exists_job();
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);

    remove_proc(shell_pid);

    printf("tsh quit :)\n");
    exit(0);
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char ** argv) {
    if(argv[1] == NULL){
        print_error("need more arguments\n");
        return;
    }else if(argv[2] != NULL){
        print_error("too many arguments\n");
        return;
    }

    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);   

    struct job_t * job = pgidjid_str2job(argv[1]);
    if(job == NULL || job->state == UNDEF){
        print_error("no such job or process group\n");
        return;
    }

    char child_stat[3];
    bool need_change_child_stat = true;

    if(strcmp(argv[0], "fg") == 0){
        strcpy(child_stat, "R+");
        change_proc_stat(shell_pid, "Ss");

        if(job->state == ST){  // ST -> FG
            kill(-job->pgid, SIGCONT);
        }
        job->state = FG;  // ST -> FG, BG -> FG

    }else if(strcmp(argv[0], "bg") == 0){  // ST -> BG, BG -> BG
        strcpy(child_stat, "R");
        if(job -> state != BG){
            kill(-job->pgid, SIGCONT);
        } else {
            need_change_child_stat = false;
        }
        job->state = BG;
       
    }

    if(need_change_child_stat) {
        for(int i = 0; i < job->proc_num; i++) {
            change_proc_stat(job->pids[i], child_stat);
        }
    }

    //unblock
    sigprocmask(SIG_SETMASK, &prev, NULL);

    if(strcmp(argv[0], "fg") == 0){
        waitfg(job->pgid);
        change_proc_stat(shell_pid, "Rs+");
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pgid) {
    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);

    struct job_t * job = getjobpgid(jobs, pgid);
    // NOTE: we need to check if job is NULL, 'cause the job could have been cleared from job list and getjobpgid returns NULL 
    while(job != NULL && job -> state == FG) {  
        sigsuspend(&prev);
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t * handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        // NOTE we can't use getpgid(pid) to get pgid, 'cause process pid might have terminated
        struct job_t * job = getjobpid(jobs, pid);

        if(WIFSTOPPED(status)){
            job -> state = ST;
        } else if(WIFSIGNALED(status)){ // WIFSIGNALED() returns true if the child process was terminated by a signal.
            int signal_num = WTERMSIG(status);
            printf("process %d terminated due to uncaught signal %d: %s\n", pid, signal_num, strsignal(signal_num));
            job -> terminated_proc_num = job->terminated_proc_num + 1;
        } else {
            // printf("terminate in other cases\n");
            (job -> terminated_proc_num)++;
        }


        if(job->terminated_proc_num == job->proc_num) {
            deletejob(jobs, job->pgid);
        }

        if(WIFSTOPPED(status)){
            change_proc_stat(pid, "T");
        } else {
            remove_proc(pid);
        }
    }
    sigprocmask(SIG_SETMASK, &prev_all, NULL);

    errno = olderrno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {
    int olderrno = errno;

    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);

    pid_t fg_pgid = fgpgid(jobs);

    if(fg_pgid != 0)
        kill(-fg_pgid, SIGINT);
    
    sigprocmask(SIG_SETMASK, &prev, NULL);  // unblock
   
    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP. Job and proc state will 
 *     be changed in SIGCHLD handler when children stop. 
 */
void sigtstp_handler(int sig) {
    int olderrno = errno;

    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);

    pid_t fg_pgid = fgpgid(jobs);
    if(fg_pgid != 0)
        kill(-fg_pgid, SIGTSTP);

    sigprocmask(SIG_SETMASK, &prev, NULL);  // unblock
   
    errno = olderrno;
    return;
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal. 
 */
void sigquit_handler(int sig) {
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/*********************
 * End signal handlers
 *********************/
