#include "job.h"
#include "tsh.h"

#include <stdlib.h>

struct job_t jobs[MAXJOBS]; /* The job list */
int nextjid = 1;            /* next job ID to allocate */

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t * job) {
    job->pgid = 0;
    job->jid = 0;
    job->proc_num = 0;
    job->terminated_proc_num = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t * jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t * jobs) {
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t * jobs, pid_t pgid, int proc_num, int state, char * cmdline, pid_t * child_pid) {
    int i;
    
    if (pgid < 1)
	    return 0;

    for (i = 0; i < MAXJOBS; i++) {  
        if (jobs[i].pgid == 0) {    // find an empty slot
            jobs[i].pgid = pgid;
            jobs[i].proc_num = proc_num;
            jobs[i].state = state;
            jobs[i].jid = nextjid++; 
            if (nextjid > MAXJOBS) 
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);

            for(int child_idx = 0; child_idx < proc_num; child_idx++) {
                jobs[i].pids[child_idx] = child_pid[child_idx]; 
            }

            if(verbose){
                printf("Added job [%d] pgid: %d %s\n", jobs[i].jid, jobs[i].pgid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    print_error("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PGID=pgid from the job list */
int deletejob(struct job_t * jobs, pid_t pgid) {
    int i;

    if (pgid < 1)
	    return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid) {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs)+1; 
            return 1;
        }
    }
    return 0;
}

/* fgpgid - Return PGID of current foreground job, 0 if no such job */
pid_t fgpgid(struct job_t * jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++){
	    if (jobs[i].state == FG) {
	        return jobs[i].pgid;
        }
    }
    return 0;
}

/* getjobpgid  - Find a job (by PGID) on the job list */
struct job_t * getjobpgid(struct job_t * jobs, pid_t pgid) {
    int i;

    if (pgid < 1)
	    return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pgid == pgid)
            return &jobs[i];
    return NULL;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t * getjobpid(struct job_t * jobs, pid_t pid) {
    int i;

    if (pid < 1)
	    return NULL;
    for (i = 0; i < MAXJOBS; i++) {
        if(jobs[i].pgid == 0) continue;

        for(int j = 0; j < jobs[i].proc_num; j++) {
            if(jobs[i].pids[j] == pid) 
                return &jobs[i];
        }
    }

    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t * getjobjid(struct job_t * jobs, int jid) {
    int i;

    if (jid < 1)
	    return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pgid2jid - Map process group ID to job ID */
int pgid2jid(pid_t pgid) {
    int i;

    if (pgid < 1)
	    return 0;
    for (i = 0; i < MAXJOBS; i++)
	    if (jobs[i].pgid == pgid) {
            return jobs[i].jid;
        }
    return 0;
}

/* find a job by PGID or JID (prefixed by %) string */ 
struct job_t * pgidjid_str2job(char * str) {
    if(str[0] == '%'){  // %JID
        char jid_str[20];
        int i;
        for(i = 0; str[i+1] != '\0'; i++){
            jid_str[i] = str[i+1];
        }
        jid_str[i] = '\0';
        int jid = atoi(jid_str);
        return getjobjid(jobs, jid);
    }else{  // PGID
        pid_t pgid = (pid_t)atoi(str); 
        return getjobpgid(jobs, pgid);
    }
}


/* listjobs - Print the job list */
void listjobs(struct job_t * jobs) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pgid);
            switch (jobs[i].state) {
            case BG: 
                printf("Running ");
                break;
            case FG: 
                printf("Foreground ");
                break;
            case ST: 
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ", 
                i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}

pid_t check_suspend() {
    int i;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == ST)
            return jobs[i].pgid;
    return 0;
}

pid_t check_run() {
    int i;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG || jobs[i].state == BG )
            return jobs[i].pgid;
    return 0;
}

pid_t check_exists_job() {
    int i;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pgid > 0)
            return jobs[i].pgid;
    return 0;
}

/******************************
 * end job list helper routines
 ******************************/
