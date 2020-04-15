/*
The idea is to write a C program that simulates a Rate Monotonic (RM) real-scheduler.
The scheduler will:
*/
/*
    - Create n number of threads
        - Based upon the tasks defined in the task set file
*/
/*
    - Simulate the scheduling of those threads using posix based semaphores
        - Each thread will wait in a while loop waiting to acquire the semaphore
        - Once acquired the thread will print-out just its task name then go wait for the next semaphore post by the scheduler (Only one function should be needed to implement the thread tasks)
        - The scheduler will release or post a semaphore (so n tasks means n sempahores) based upon the RM scheduling algorithm
        - A “clock tick” will be simulated through each iteration of the main scheduling loop (i.e. one iteration first clock tick, two iterations, second clock tick,)
    - Assume all task are periodic and released at time 0.


The RM scheduler program (rmsched.c) that takes three arguments from the command line (no prompting the user from within the program):
    - ./rmsched <nperiods> <task set> <schedule>
        - <nperiods> defines the number of hyperperiods
        - <task set> is a file containing the task set descriptions
        - <scheduler> is a file that contains the actual schedule
    - The format of the <task set> file is as follows:
        - T1 2 6
          T2 3 12
          T3 6 24
        - The first column represents the task name
        - The second column represents the task WCET
        - The third column represents the task period
    - The example format of the <schedule> file is as follows:
        - 0  1  2  3  4  5  6  7  8  9 10  11 12 13 14 15
          T1 T1 T2 T2 T2 T3 T1 T1 T3 T3 T3 T3 T1 T1 T2 T2
        - The first row represents the time (only needs to display once at the top of the file)
        - The second row represents the actual schedule for one hyperperiod
        - The next hyperperiod should begin on the next row
    - The main process:
        - Create the n threads and the n semaphores
        - Determine which thread to release based upon the RM scheduling algorithm
        - Check to ensure that the task set is actually schedulable before attempting to create a schedule.
    - The task thread only has to wait for the appropriate semaphore then print-out its name to the <schedule> file and wait for the next semaphore post.
*/

#define NUMBER 3
#define STR_LENGTH 32

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <string.h>

typedef struct  {
    char* name;
    int wcet;
    int period;
} proc;

typedef struct {
    proc* p;
    int num;
} proc_holder;

void *threadFun(void*);

proc* createProcess(proc*, char*, int, int);
void deleteProcs(proc_holder*);
void createProcs(char*, proc_holder*);
void printProc(proc_holder*);
void sortArray(proc_holder*);
void initSem();

// Threading
sem_t* sem;
struct proc* p;
char* fp;
proc_holder ph;

/*
The producer/consumer program (prodcon.c) that takes three arguments from the
command line (no prompting the user from within the program).
*/
int main(int argc, char *argv[]) {
    fp = strdup("./testFile.txt");
    pthread_t* tid;

    createProcs(fp, &ph);
    initSem();

    tid = malloc(sizeof(pthread_t)*ph.num);
    for (int i = 0; i < ph.num; ++i) {
        int* temp = malloc(sizeof(int));
        *temp = i;
        pthread_create(&tid[i],NULL,threadFun,temp);
    }

    for (int i = 0; i < ph.num; ++i) {
        pthread_join(tid[i],NULL);
    }


    // printProc(&ph);

    deleteProcs(&ph);
    free(tid);
    free(sem);
    return 0;
}

void *threadFun(void* param) {
    int id = *((int *) param);
    sem_wait(&sem[id]);
    printf("%s %d %d\n", ph.p[id].name, ph.p[id].wcet, ph.p[id].period);

    free(param);
    pthread_exit(0);
}

void initSem() {
    sem = malloc(sizeof(sem_t)*ph.num);
    for (int i = 0; i < ph.num; ++i) {
        if(sem_init(&sem[0],0,0) == -1) {
            printf("%s\n",strerror(errno));
        }
    }
}

proc* createProcess(proc* p, char* name, int wcet, int period) {
    p->name = name;
    p->wcet = wcet;
    p->period = period;
    return p;
}

void createProcs(char* fp, proc_holder* ph) {
    FILE* fptr;

    if((fptr = fopen(strdup(fp), "r+")) == NULL) {
        printf("No such file\n");
        exit(1);
    }

    char tempName[STR_LENGTH];
    int wcet, period;

    // gets the number of lines
    // Try to reduce later
    ph->num = 0;
    int ret;
    while(1) {
        ret = fscanf(fptr, "%s %d %d", tempName, &wcet, &period);
        if(ret == 3)
            ++ph->num;
        else if(errno != 0) {
            perror("scanf:");
            break;
        } else if(ret == EOF) {
            break;
        } else {
            printf("No match.\n");
        }
    }

    rewind(fptr);
    ph->p = malloc(sizeof(proc)*ph->num);
    ph->num=0;

    while(1) {
        char* n = malloc(sizeof(char)*STR_LENGTH);
        ret = fscanf(fptr, "%s %d %d", n, &wcet, &period);
        if(ret == 3)
            createProcess(&ph->p[ph->num++], n, wcet, period);
        else if(errno != 0) {
            perror("scanf:");
            break;
        } else if(ret == EOF) {
            break;
        } else {
            printf("No match.\n");
        }
    }

    sortArray(ph);
}

void deleteProcs(proc_holder* ph) {
    for(int i = 0; i < ph->num; ++i) {
        free(ph->p[i].name);
    }
    free(ph->p);
}

void printProc(proc_holder* ph) {
    for(int i = 0; i < ph->num; ++i) {
        printf("%s %d %d\n", ph->p[i].name, ph->p[i].wcet, ph->p[i].period);
    }
}

void sortArray(proc_holder* ph) {
    proc p;
    for(int i = 0; i < ph->num; ++i) {
        for (int k = 0; k < ph->num; ++k) {
            if(ph->p[i].period < ph->p[k].period) {
                p = ph->p[i];
                ph->p[i] = ph->p[k];
                ph->p[k] = p;
            }
        }
    }

}
