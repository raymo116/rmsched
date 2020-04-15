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
    int current;
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
int runSim();
int checkArray(int);
int lcm();
int max();
int checkIfRunable();

// Threading
sem_t* sem;
sem_t* mainSem;
struct proc* p;
char* taskSet;
proc_holder ph;
int running = 1;
int nPeriods;
int l;
int active;

/*
The producer/consumer program (prodcon.c) that takes three arguments from the
command line (no prompting the user from within the program).
*/
int main(int argc, char *argv[]) {
    taskSet = strdup("./testFile.txt");
    nPeriods = 3;
    pthread_t* tid;

    createProcs(taskSet, &ph);
    initSem();

    tid = malloc(sizeof(pthread_t)*ph.num);
    for (int i = 0; i < ph.num; ++i) {
        int* temp = malloc(sizeof(int));
        *temp = i;
        pthread_create(&tid[i],NULL,threadFun,temp);
    }

    l = lcm();

    if(checkIfRunable()<=1)
        runSim();
    else
        printf("This is unable to be scheduled\n");


    for (int i = 0; i < ph.num; ++i) {
        pthread_join(tid[i],NULL);
    }


    // printProc(&ph);

    deleteProcs(&ph);
    free(tid);
    free(sem);
    return 0;
}

int runSim() {
    int* stack = malloc(sizeof(int)*ph.num);
    int top_s= -1;

    // Adds all processes to a stack in reverse order of importance
    for(int i = ph.num-1; i > -1; --i) {
        stack[++top_s] = i;
    }

    sem_post(&sem[stack[top_s]]);
    sem_wait(mainSem);


    for(int t = 1; t < l; ++t) {
        for(int i = 0; i < ph.num; ++i) {
            if(t%ph.p[i].period == 0) {

                if(ph.p[i].current == 0) {
                    stack[++top_s] = i;
                    __sync_add_and_fetch(&ph.p[i].current, &ph.p[i].wcet);
                }
                else {
                    printf("There was an error scheduling\n");
                    return -1;
                }
            }
        }
        if(top_s != -1) {
            sem_post(&sem[stack[top_s]]);
            sem_wait(mainSem);
            fflush(stdout);
        }

        if (ph.p[stack[top_s]].current == 0) --top_s;
    }

    running = 0;
    free(stack);
    return 0;
}

void *threadFun(void* param) {
    int id = *((int *) param);
    while(running) {
        sem_wait(&sem[id]);
        if(running) {
            printf("%s ", ph.p[id].name);
            fflush(stdout);
            __sync_sub_and_fetch(&ph.p[id].current, 1);
            sem_post(mainSem);
        }
    }

    free(param);
    pthread_exit(0);
}

void initSem() {
    mainSem = malloc(sizeof(sem_t));
    if(sem_init(mainSem,0,0) == -1) {
            printf("%s\n",strerror(errno));
    }

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
    p->current = wcet;
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

int checkArray(int lcm) {
    for(int i = 0; i < ph.num; ++i) {
        if(lcm%(ph.p[i].period != 0)) return 0;
    }
    return 1;
}

int lcm() {
    int l = max();
    while(checkArray(l) != 1) ++l;
    return l;
}

int max() {
    int max = 0;
    for(int i = 0; i < ph.num; ++i) {
        if(ph.p[i].period > max) max = ph.p[i].period;
    }
    return max;
}

int checkIfRunable() {
    int sum = 0;
    for(int i = 0; i < ph.num; ++i) {
        sum += ph.p[i].wcet/ph.p[i].period;
    }
    return sum;
}
