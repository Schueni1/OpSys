/**
 * @file server.c
 * @author Aaron Duxler 01427540 <e1427540@student.tuwien.ac.at>
 * @date 29.05.2017
 *
 * @brief This Program creates a shared memory object, if it does not already exist. Otherwise
    it exits with an error. 
    After creation of the shared memory object it waits for clients to connect to it, and answers it's request
     using the same shared memory object. The database has to be parsed as a file through command line argument. The database is stored as linkedList.
     The server can handle the signal SIGINT, SIGTERM and SIGUSR1. Up on receipt of SIGINT or SIGTERM the server terminates, on SIGUSR1 it prints the
     whole database (content of the linkedList).
 **/

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>

/* @brief Permission used for sem_open call (owner can read and write) */
#define PERMISSION (0600)

/* @brief Name of the shared memory Object */
#define SHM_NAME "01427540_shm"

/* @brief names of the semaphores */
#define SEM_1 "/01427540_sem_1"
#define SEM_2 "/01427540_sem_2"

/* @brief Length of an array*/
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0])) 

typedef struct node { /** < The struct for the LinkedList*/
    unsigned int pid;
    unsigned int cpu;
    unsigned int mem;
    unsigned int runtime;
    char *command;
    struct node *next;
} node_t;

struct myshm { /** < shared memory object*/ 
    enum OPERATION {pid_op, sum, min, max, avg} op;
    enum REQUEST {cpu, mem, runtime, command, pid_req} req;
    int pid_value;
    char answer[128];
};

/**
* This variable is set upon receipt of a signal 
*/
volatile sig_atomic_t quit = 0; 

/**
* stores address of the first element (head) of the LinkedList
*/
node_t *head = NULL;

/**
* stores return value of shm_open (non negativ file discriptor on success)
*/
int shmfd = -1;

/**
* stores addresses of the semaphores 1 and 2 on succesful call of sem_open (SEM_FAILED otherwise);
*/
sem_t *s1 = SEM_FAILED, *s2 = SEM_FAILED;

/** 
* Name of the program 
*/
static const char *progname; 

/**
* @brief prints usage on stderr. Exits program with EXIT_FAILURE
* @details global variable: progname
*/
static void usage();

/**
 * @brief Opens a new file discroptpr based on the parsed argument. Calls usage if argument count is incorrect or was unable to open file.
 * @param argc The argument counter
 * @param argv The argument vector
 * @details global variable progname
 * @return on success returns struct file discriptor of parsed file
 */
static FILE* parse_args(int argc, char **argv);

/**
 * @brief pushes a new node to the linked list
 * @param command
 * @param pid
 * @param cpu
 * @param mem
 * @param runtime
 * @details global variables: head, progname
 */
static void push(char *command, unsigned int pid, unsigned int cpu, unsigned int mem, unsigned int runtime);

/**
 * @brief frees linkedList.
 * @details global variable: head
 */
static void free_list(void);

/**
 * @brief unlinks shared memory object and semaphores. Calls free_list
 * @details global variable: progname
 */
void free_resources(void);

/**
 * @brief reads from file and pushes to linked list. Prints error message exits program, if there is mistake in the file.
 * @param file pointer to parsed file;
 * @details global variable: progname
 */
static void csvParseAndPush(FILE *file);

/**
 * @brief creats shared memory object and semaphores. Fails, if shared memory object or semaphores already exist. Calls communicate function.
 * @details global variable: progname, s1, s2, shmfd
 */
static void create_shared(void);

/**
 * @brief synchronises communication between client and server and checks if signals have been recieved.
    Calls fetch_info and writes answer to the request into the shared memory object. 
 * @param shared points to shared memory object.
 * @details global variable: progname, quit, s1, s2
 */
static void communicate(struct myshm *shared);

/**
 * @brief calls the function for the desired operation.
 * @param op operation code (sum, min, avg, ...)
 * @param req request code (cpu, mem, time, ...)
 * @param pid_val pid if operation is pid
 * @details global variable: progname
 * @return string as answer for the client
 */
static char* fetch_info(enum OPERATION op, enum REQUEST req, int pid_val);

/**
 * @brief prints the whole linked list line by line
 * @details global variable: head
 */
static void print_list(void);

/**
 * @brief returns a the request of the desired pid by iterating over the linkedList or returns a message if no such pid was found.
 * @details global variable: head
 * @return returns a the request of the desired pid or a message if no such pid was found.
 */
static char* get_pid_info(int pid_val, enum REQUEST req);

/**
 * @brief sums all entries of the the desired request.
 * @details global variable: head
 * @return returns the sum of all entries for the request.
*/
static unsigned int sum_request(enum REQUEST req);

/**
 * @brief searches and returns the maximum value of the the desired request by iterating over the linkedlist.
 * @details global variable: head
 * @return returns the maximum value for the disired request.
*/
static unsigned int max_request(enum REQUEST req);

/**
 * @brief searches and returns the minimum value of the the desired request by iterating over the linkedlist.
 * @details global variable: head
 * @return returns the minimum value for the disired request.
*/
static unsigned int min_request(enum REQUEST req);

/**
 * @brief calculates and returns the arithmetic average value of the the desired request by iterating over the linkedlist.
 * @details global variable: head
 * @return returns the arithmetic average value for the disired request as an unsigned int.
*/
static unsigned int avg_request(enum REQUEST req);

/**
 * @brief converts time in seconds to hour minutes seconds format
 * @param seconds contains time i seconds
 * @param answer is the char array to save the calculated time
*/
static void convert_time(unsigned int seconds, char **answer);

/**
 @brief creates final answer format. 
 @param answer is the buffer where the answer will be saved.
 @param req defines the request
 @param op_function function pointer to a numeric operation function
 @return returns the converted answer.
*/
static char* convert_answer_format(char **answer,enum REQUEST req, unsigned int (*op_function)(enum REQUEST req));

/**
* @brief sets up signal handler to ignore all signals that can be caught except the signals SIGINT, SIGTERM and SIGUSR1
* @details global variable progname
*/
static void setup_signal_handler(void);

/**
* @brief sets global variable quit to the signal id received
* @param sig Signal number catched.
* @details global variable quit
*/
static void signal_handler(int sig);

/**
*Program entry point
*@brief calls atexit() to set function free_resources() as exit function. Calls setup_signal_handler(), parse_args() and create_shared(). 
*@param argc The argument counter
*@param argv The argument vector
*@return Returns EXIT_SUCCESS if atexit call is successful and non of the called functions has a failure. Otherwise it returns EXIT_FAILURE.
*/
int main(int argc, char **argv){
    if (atexit(free_resources) != 0){
        (void)fprintf(stderr, "%s can not set exit function\n", progname);
        exit(EXIT_FAILURE);
    }
    setup_signal_handler();
    csvParseAndPush(parse_args(argc, argv));
    create_shared();
    exit(EXIT_SUCCESS);
}

static void create_shared(void){
    struct myshm *shared;
    /* create shared memory object */
    shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL,PERMISSION);
    if (shmfd == -1){
        (void)fprintf(stderr, "%s shm_open fail!\n", progname);
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shmfd, sizeof *shared) == -1){
         (void)fprintf(stderr, "%s ftruncate fail!\n", progname);
         exit(EXIT_FAILURE);
    }
    shared = mmap(NULL, sizeof *shared,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        shmfd, 0);
    if (shared == MAP_FAILED){
         (void)fprintf(stderr, "%s mmap fail!\n", progname);
         exit(EXIT_FAILURE);
    }
    if (close(shmfd) == -1){
        (void)fprintf(stderr, "%s close fail!\n", progname);
        exit(EXIT_FAILURE);
    }

    s1 = sem_open(SEM_1, O_CREAT | O_EXCL, PERMISSION, 1);
    if(s1==SEM_FAILED){
        (void)fprintf(stderr, "sem_open 1 fail\n");
        exit(EXIT_FAILURE);
    }
    s2 = sem_open(SEM_2, O_CREAT | O_EXCL, PERMISSION, 0);
    if(s2==SEM_FAILED){
        (void)fprintf(stderr, "sem_open 2 fail\n");
        exit(EXIT_FAILURE);
    }

    communicate(shared);
    
    if (munmap(shared, sizeof *shared) == -1){
        (void)fprintf(stderr, "%s mumnap fail!\n", progname);
        exit(EXIT_FAILURE);
    }
}

static void communicate(struct myshm *shared){
    
    while(!quit){
        if(sem_wait(s2)==-1 && !quit){
            (void)fprintf(stderr, "%s sem_wait fail\n", progname);
            exit(EXIT_FAILURE);
        }
        if(quit==SIGTERM || quit==SIGINT){
            break;
        }
        else if(quit){
            print_list();
            quit = 0;
            continue;
        }
        char *answer = fetch_info(shared->op, shared->req, shared->pid_value); 
        (void)strcpy(shared->answer, answer!=NULL ? answer : "Unvalid request");
        
        if(sem_post(s1)==-1){
            (void)fprintf(stderr, "%s sem_post fail\n", progname);
            if(answer!=NULL){
                free(answer);
            }
            exit(EXIT_FAILURE);
        }
        
        if(answer!=NULL){
            free(answer);
        }
    }
}

static void print_list(void){
    for(node_t *current = head; current!=NULL; current = current->next){
        (void)printf("%u,%u,%u,%u,%s\n", current->pid, current->cpu, current->mem, current->runtime, current->command);
    }
}

static char* convert_answer_format(char **answer,enum REQUEST req, unsigned int (*op_function)(enum REQUEST req)){
    switch (req){
        case runtime: 
            convert_time(op_function(req), answer);
            return *answer;
        case pid_req: 
            (void)sprintf(*answer, "- %u", op_function(req));
            return *answer;
        default: 
            (void)sprintf(*answer, "- %.1Lf", (long double) op_function(req)/10);
            return *answer;
    }
}

static char* fetch_info(enum OPERATION op, enum REQUEST req, int pid_val){ 
    char *answer = malloc(sizeof(char*)*64);
    if(answer==NULL){
        (void)fprintf(stderr, "malloc answer failed\n");
        exit(EXIT_FAILURE);
    }
    switch (op){
        case pid_op: 
            free(answer);
            return get_pid_info(pid_val, req);
        case sum: 
            return convert_answer_format(&answer, req, sum_request);
        case min: 
            return convert_answer_format(&answer, req, min_request);
        case max: 
            return convert_answer_format(&answer, req, max_request);
        case avg: 
            return convert_answer_format(&answer, req, avg_request);
        default: 
            assert(0); /*unreachable all cases covered*/
    }
    return NULL; /*unreachable*/
}

static unsigned int max_request(enum REQUEST req){
    unsigned int max = 0;
    for(node_t *current = head; current!=NULL; current = current->next){
        switch(req){
            case mem: 
                if(current->mem > max){
                    max = current->mem;
                } 
                break;
            case runtime: 
                if(current->runtime > max){
                    max = current->runtime;
                };
                break;
            case cpu: 
                if(current->cpu > max){
                    max = current->cpu;
                }
                break;
            case pid_req:
                if(current->pid > max){
                    max = current->pid;
                }
                break;
            default: 
                assert(0); /*if 'command' assert(0), client should catch this case*/
        }
    }
    return max;
}

static unsigned int min_request(enum REQUEST req){
    unsigned int min = UINT_MAX;
    for(node_t *current = head; current!=NULL; current = current->next){
       switch(req){
            case mem: 
                if(current->mem < min){
                    min = current->mem;
                } 
                break;
            case runtime: 
                if(current->runtime < min){
                    min = current->runtime;
                };
                break;
            case cpu: 
                if(current->cpu < min){
                    min = current->cpu;
                }
                break;
            case pid_req:
                if(current->pid < min){
                    min = current->pid;
                }
                break;
            default: 
                assert(0); /*if 'command' assert(0), client should catch this case*/
        }
    }
    return min;
}

static unsigned int sum_request(enum REQUEST req){
    unsigned int sum = 0;
    for(node_t *current = head; current!=NULL; current = current->next){
        switch (req){
            case mem: 
                sum += current->mem;
                break;
            case runtime: 
                sum += current->runtime;
                break;
            case cpu: 
                sum += current->cpu;
                break;
            case pid_req:
                sum += current->pid;
                break;
            default: 
                assert(0); /*if 'command' assert(0), client should catch this case*/
        }
    }
    return sum;
}

static unsigned int avg_request(enum REQUEST req){
    unsigned int sum = 0;
    int i = 0;
    for(node_t *current = head; current!=NULL; current = current->next){
        ++i;
        switch (req){
            case mem: 
                sum += current->mem;
                break;
            case runtime: 
                sum += current->runtime;
                break;
            case cpu: 
                sum += current->cpu;
                break;
            case pid_req:
                sum += current->pid;
                break;
            default: 
                assert(0); /*if 'command' assert(0), client should catch this case*/
        }
    }
    return sum/i;
}

static char* get_pid_info(int pid_value, enum REQUEST req){
    char *answer = malloc(sizeof(char*)*64);
    if(answer==NULL){
        (void)fprintf(stderr, "malloc answer fail\n");
        exit(EXIT_FAILURE);
    }
    for(node_t *current = head; current != NULL; current = current->next){
        if(current->pid == pid_value){
            switch(req){
                case pid_req: 
                    (void)sprintf(answer, "%u %u", current->pid, current->pid);
                    return answer;
                case cpu: 
                    (void)sprintf(answer, "%u %.1Lf", current->pid, (long double)current->cpu/10);
                    return answer;
                case mem: 
                    (void)sprintf(answer, "%u %.1Lf", current->pid, (long double)current->mem/10);
                    return answer;
                case runtime:
                    convert_time(current->runtime, &answer);
                    return answer;
                case command: 
                    (void)sprintf(answer, "%u %s", current->pid, current->command);
                    return answer;
                default: 
                    assert(0); /*unreachable all cases covered*/
            }
        }
    }
    (void)sprintf(answer, "%u %s", pid_value, "No such pid found");
    return answer;
}

static void convert_time(unsigned int all_seconds, char **answer){
    long double hours = (all_seconds)/3600.;
    long double minutes = ((all_seconds)/3600. - (unsigned int)hours)*60.;
    long double seconds = (((all_seconds)/3600. - (unsigned int)hours)*60 - (unsigned int)minutes)*60;
    (void)sprintf(*answer, "%uh %um %.0Lfs", (unsigned int)hours, (unsigned int)minutes, seconds);
}

static FILE* parse_args(int argc, char **argv){
    progname = argv[0];
    if(argc!=2){
        usage();
    }
    FILE *file = fopen(argv[1], "r");
    if(file==NULL){
        (void)fprintf(stderr, "%s open file fail\n", progname);
        usage();
    }
    return file;
}

static void usage(){
    (void)fprintf(stderr, "%s usage:\n", progname);
    (void)fprintf(stderr, "%s <file>\n", progname);
    exit(EXIT_FAILURE);
}

static void csvParseAndPush(FILE *file){
    char line[128]; /*stores the whole line*/
    bzero(line, sizeof(line));
    int start; /*needed to create new substrings of line*/
    char *status; /*return value of strtol*/
    unsigned int line_number = 0;  /*saves line number to print in case of an error*/
    while(fgets(line, 1022, file)!=NULL){
        ++line_number;
        start = 0; 
        int count = 0; /*marks the entry in the file*/
        char tempBuff[128]; /*stores the substrings of line during progression*/
        unsigned int pid, cpu, mem, runtime;
        char command[64];
        for(int i = start; line[i]!='\n' && i<128; ++i){
            if(line[i] == ','){
              (void)strcpy(tempBuff, line+start); /*sets new start of the string*/
              tempBuff[i-start] = '\0'; /*sets new end of the string*/
              switch(count){
                  case 0:     
                    pid = strtol(tempBuff, &status, 10);
                    if(strcmp(status, "\0")!=0){
                        (void)fprintf(stderr, "%s pid entry in line number %d\n please correct your file\n", progname, line_number);
                        exit(EXIT_FAILURE);
                    }
                    break;
                  case 1:
                    cpu = strtol(tempBuff, &status, 10);
                    if(strcmp(status, "\0")!=0){
                        (void)fprintf(stderr, "%s cpu entry in line number %d\n please correct your file\n", progname, line_number);
                        exit(EXIT_FAILURE);
                    } 
                    break;
                  case 2:
                    mem = strtol(tempBuff, &status, 10); 
                    if(strcmp(status, "\0")!=0){
                        (void)fprintf(stderr, "%s mem entry in line number %d\n please correct your file\n", progname, line_number);
                        exit(EXIT_FAILURE);
                    }
                    break;
                  case 3:
                    runtime = strtol(tempBuff, &status, 10);
                    if(strcmp(status, "\0")!=0){
                        (void)fprintf(stderr, "%s time entry in line number %d\n please correct your file\n", progname, line_number);
                        exit(EXIT_FAILURE);
                    } 
                    break;
              }
              start = i+1; /*+1 to ignore next ','*/
              ++count;
            }  
        }
        if(start!=0){ /*checks if it is the last entry of the line*/
            line[strlen(line)-1] = '\0'; /*removes new line '\n'*/
            (void)strcpy(command, line+start);
            push(command, pid, cpu, mem, runtime);
        }
    }   
    fclose(file);
}

static void push(char *command, unsigned int pid, unsigned int cpu, unsigned int mem, unsigned int runtime){
        node_t *new_node;
        new_node = (node_t *) malloc(sizeof(node_t));
        if(new_node==NULL){
            (void)fprintf(stderr, "%s: malloc push failed\n", progname);
            exit(EXIT_FAILURE);
        }
        new_node->command = malloc(sizeof(char*)*64);
        if(new_node->command==NULL){
            (void)fprintf(stderr, "%s: malloc push new command failed\n", progname);
            exit(EXIT_FAILURE);
        }
        (void) strcpy(new_node->command, command);
        new_node->pid = pid; new_node->cpu = cpu; new_node->mem = mem; new_node->runtime = runtime;
        new_node->next = head;
        head = new_node;  
}

void free_resources(void){
    if(sem_close(s1)==-1){
        (void)fprintf(stderr, "%s close SEM_1 fail\n", progname);
    }
    if(sem_close(s2)==-1){
        (void)fprintf(stderr, "%s close SEM_2 fail\n", progname);
    }
    if (shmfd != -1) {
        if (shm_unlink(SHM_NAME) == -1)
        (void)fprintf(stderr, "%s shm_unlink fail\n", progname);
    }
    if (s1 != SEM_FAILED) {
        if (sem_unlink(SEM_1) == -1)
        (void)fprintf(stderr, "%s sem_unlink 1 fail\n", progname);
    }
    if (s2 != SEM_FAILED) {
        if (sem_unlink(SEM_2) == -1)
        (void)fprintf(stderr, "%s sem_unlink 2 fail\n", progname);
    }
    free_list();
}

static void free_list(void){
    node_t * current = head;
    node_t * prev;
    while (current != NULL) {
        prev = current;
        current = current->next;
        free(prev->command);
        free(prev);
    }
}

static void setup_signal_handler(void){
    const int signals[] = {SIGINT, SIGTERM, SIGUSR1};
    struct sigaction s;

    s.sa_flags = 0;
    s.sa_handler = signal_handler;
    if(sigfillset(&s.sa_mask) < 0) {
        (void)fprintf(stderr, "%s sigfillset", progname);
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < COUNT_OF(signals); i++) {
        sigdelset(&s.sa_mask, signals[i]);
        if (sigaction(signals[i], &s, NULL) < 0) {
            (void)fprintf(stderr, "%s sigaction", progname);
            exit(EXIT_FAILURE);
        }
    }
    if(sigprocmask(SIG_SETMASK, &s.sa_mask, NULL) != 0){
        (void)fprintf(stderr, "%s sigprcmask", progname);
        exit(EXIT_FAILURE);
    }
}

static void signal_handler(int sig)
{    
    quit = sig;
}