#ifndef JOB_H
#define JOB_H

#include "tsh.h"
#include <sys/types.h>

#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

struct job_t {              /* the job struct */
    pid_t pgid;             /* PGID */  // 
    int jid;                /* job ID [1, 2, ...] */
    pid_t pids[MAXPIPES];   /* PIDs in this job */         
    int proc_num;           /* length of arrary pids */
    int terminated_proc_num;/* already terminated processes in this job */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};

extern struct job_t jobs[MAXJOBS];
extern int nextjid;

void clearjob(struct job_t * job);
void initjobs(struct job_t * jobs);
int maxjid(struct job_t * jobs); 
int addjob(struct job_t * jobs, pid_t pgid, int proc_num, int state, char * cmdline,  pid_t * child_pid);
int deletejob(struct job_t * jobs, pid_t pid); 
pid_t fgpgid(struct job_t * jobs);
struct job_t *getjobpgid(struct job_t * jobs, pid_t pgid);
struct job_t *getjobpid(struct job_t * jobs, pid_t pid);
struct job_t *getjobjid(struct job_t * jobs, int jid); 
int pgid2jid(pid_t pgid); 
struct job_t * pgidjid_str2job(char * str);
void listjobs(struct job_t * jobs);

pid_t check_suspend();
pid_t check_run();
pid_t check_exists_job();

#endif 