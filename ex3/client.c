/**
 * @file client.c
 * @author Aaron Duxler 01427540 <e1427540@student.tuwien.ac.at>
 * @date 31.05.2017
 *
 * @brief This Program connects to a shared memory object if it does not already exist. Otherwise
    it exits with an error. Writes request to the server using the shared memory object and prints the answer on stdout.
    Request can be passed to the client by stdin or by argument. If requests are passed by stdin, the client waits for input in a loop,
    and sends the request to the sever. The client terminates if it receives an 'end of file' a SIGINT or a SIGTERM signal.
 **/

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>

/* @brief Permission used for sem_open call (owner can read and write) */
#define PERMISSION (0600)

/* @brief Name of the shared memory Object */
#define SHM_NAME "01427540_shm"

/* @brief names of the semaphores */
#define SEM_1 "/01427540_sem_1"
#define SEM_2 "/01427540_sem_2"

/* @brief Length of an array*/
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0])) 


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
* Name of the program 
*/
static const char *progname;

/**
* @brief prints usage on stderr. Exits program with EXIT_FAILURE
* @details global variable: progname
*/
static void usage();

/**
* @brief saves progname and calls usage if argument count is incorrect.
* @param argc argument count
* @param argv argument vector
* @details global variable: progname
*/
static void parse_args(int argc, char **argv);

/**
* @brief connects to shared memory object and calls communicate function.
* @param argc argument count, needs to be passed to comminicate function
* @param argv argument vector, needs to be passed to communicate function
* @detail global variable progname
*/
static int connect_shared(int argc, char **argv);

/**
* @brief opens semaphores. If arguments are have been passed, it calls send_receive once. Otherwise it reads from stdin and calls
sep_op_req and send and receive in a loop, until client receives an 'end of file' or a SIGINT, SIGTERM signal.
* @param shared points to shared memory object
* @param argc argument count needed to decide between calling in a loop or once.
* @param argv argument vector containing operation and request, if they were passed as an argument.
* @detail global variable progname, quit
*/
static int communicate(struct myshm *shared, int argc, char **argv);

/**
* @brief separates string buff at the first ' ' into to substrings and saves them to operation and request.
* @param buf contains the whole line
* @param operation stores the operation which should be buf until first ' '
* @param request stores the request which should be buf from first ' ' until the end of the line
* @param endptr indicates if operation is a pid or not
* @param pid stores the pid if operation is a number
* @detail global variable progname
* @return true on success, false if request, command or the combination of both is unvalid.
*/
static bool sep_op_req(char *buf, char **operation, char **request, char **endptr, unsigned int *pid);

/**
* @brief writes request and operation to shared memory object
* @param shared points to shared memory object
* @param operation stores the operation
* @param request stores the request
* @param endptr indicates if operation is a pid or not. If it is a pid, pid_op and pid_value will be set in shared memory.
* @param pid stores the pid if operation is a number
*/
static void write_to_shared(struct myshm *shared, char *operation, char *request, char *endptr, int pid);

/**
* @brief synchronizes communciation with server, calls write_to_shared() and prints the servers answer on stdout.
* @param shared points to shared memory object
* @param operation stores the operation
* @param request stores the request
* @param endptr indicates if operation is a pid or not.
* @param pid stores the pid if operation is a number
*/
static void send_receive(struct myshm *shared, sem_t *s1, sem_t *s2, char *operation, char *request, char *endptr, unsigned int pid);

/**
* @brief checks if operation, request and the combination of both is valid
* @param operation stores operation
* @param request stores the request
* @param endptr indicates if operation is a pid or not
* @details global variable progname
* @return returns true on success and false if the request operation combination is not valid
*/
static bool check_op_req(char *operation, char *request, char *endptr);

/**
* @brief sets up signal handler to ignore all signals that can be caught except the signals SIGINT and SIGTERM
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
*@brief calls setup_signal_handler(), parse_args() and connect_shared(). 
*@param argc The argument counter
*@param argv The argument vector
*@return Returns EXIT_SUCCESS if no called functions has a failure. Otherwise it returns EXIT_FAILURE.
*/
int main(int argc, char **argv){
    setup_signal_handler();
    parse_args(argc, argv);
    exit(connect_shared(argc, argv));
}

static int connect_shared(int argc, char **argv){
    struct myshm *shared;
    /* create and/or open shared memory object */   
    int shmfd = shm_open(SHM_NAME, O_RDWR,PERMISSION);
    if (shmfd == -1){
        (void)fprintf(stderr, "%s shm_open fail!\n", progname);
        (void)fprintf(stderr, "%s server is probably not running\n", progname);
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
    
    int ret = communicate(shared, argc, argv);

    if (munmap(shared, sizeof *shared) == -1){
        (void)fprintf(stderr, "%s mumnap fail!\n", progname);
        exit(EXIT_FAILURE);
    }
    return ret;
}

static int communicate(struct myshm *shared, int argc, char **argv){
    sem_t *s1 = sem_open(SEM_1, 0);
    if(s1==SEM_FAILED){
        (void)fprintf(stderr, "%s sem_open 1 fail\n", progname);
        exit(EXIT_FAILURE);
    }
    sem_t *s2 = sem_open(SEM_2, 0);
    if(s1==SEM_FAILED){
        (void)fprintf(stderr, "%s sem_open 2 fail\n", progname);
        exit(EXIT_FAILURE);
    }
    char *endptr; 
    unsigned int pid;
    if(argc==3){
        pid = (unsigned int) strtol(argv[1], &endptr, 10);
        if(!check_op_req(argv[1], argv[2], endptr)){
            sem_close(s1); sem_close(s2);
            exit(EXIT_FAILURE);
        }
        send_receive(shared, s1, s2, argv[1], argv[2], endptr, pid);
    }
    else{
        char *buf = malloc(sizeof(char*)*128);
        char *operation = malloc(sizeof(char*)*128);
        char *request = malloc(sizeof(char*)*128);
    
        if(buf==NULL || operation==NULL || request==NULL){
            (void)fprintf(stderr, "%s buf | operation | request malloc fail\n", progname);
            exit(EXIT_FAILURE);
        }
        bzero(operation, 127); bzero(request, 127); bzero(buf, 127);
        char *op_addr = operation, *req_addr = request; /*saves addresses so they can be freed later*/
        while(fgets(buf, 127, stdin)!=0 && !quit){
           if(!sep_op_req(buf, &operation, &request, &endptr, &pid)){
             continue; /*Unvalid request*/
          }
            send_receive(shared, s1, s2, operation, request, endptr, pid);
        }
        free(buf); free(op_addr); free(req_addr);
    }
    int ret = EXIT_SUCCESS;
    if(sem_close(s1)==-1){
        (void)fprintf(stderr, "%s close SEM_1 fail\n", progname);
        ret = EXIT_FAILURE;
    }
    if(sem_close(s2)==-1){
        (void)fprintf(stderr, "%s close SEM_2 fail\n", progname);
        ret = EXIT_FAILURE;
    }
    return ret;
}

static void send_receive(struct myshm *shared, sem_t *s1, sem_t *s2, char *operation, char *request, char *endptr, unsigned int pid){
        if(sem_wait(s1)!=0){ /*prevent other clients from interfering*/
            (void)fprintf(stderr, "%s sem_wait fail\n", progname);
            exit(EXIT_FAILURE);
        }
        write_to_shared(shared, operation, request, endptr, pid);
        if(sem_post(s2)!=0){ /*start server*/
            (void)fprintf(stderr, "%s sem_post fail\n", progname);
            exit(EXIT_FAILURE);
        }

        if(sem_wait(s1)!=0){ /*checks if server is done writing and prevents other clients from interfering*/
            (void)fprintf(stderr, "%s sem_wait fail\n", progname);
            exit(EXIT_FAILURE);
        }
        (void)printf("%s\n", shared->answer);
        if(sem_post(s1)!=0){ /*gives next client permission to write to shared memory*/
            (void)fprintf(stderr, "%s sem_post fail\n", progname);
            exit(EXIT_FAILURE);
        }
}

static void write_to_shared(struct myshm *shared, char *operation, char *request, char *endptr, int pid){
        if(strcmp(endptr, "\0")==0){
            shared->op = pid_op;
            shared->pid_value = pid;
        }
        else if(strcmp(operation, "sum")==0){
            shared->op = sum;
        }
        else if(strcmp(operation, "min")==0){
            shared->op = min;
        }
        else if(strcmp(operation, "max")==0){
            shared->op = max;
        }
        else if(strcmp(operation, "avg")==0){
            shared->op = avg;
        }

        
        
        if(strcmp(request, "pid")==0){
            shared->req = pid_req;
        }
        else if(strcmp(request, "cpu")==0){
            shared->req = cpu;
        }
        else if(strcmp(request, "mem")==0){
            shared->req = mem;
        }
        else if(strcmp(request, "time")==0){
            shared->req = runtime;
        }
        else if(strcmp(request, "command")==0){
            shared->req = command;
        }
}

static bool sep_op_req(char *buf, char **operation, char **request, char **endptr, unsigned int *pid){
    for(int i = 0; i<strlen(buf); ++i){
        if(buf[i]==' '){
            buf[i] = '\0'; /*marks new end of the string*/
            *operation = buf;
            *request = &((*operation)[i+1]); /*marks new start of the string*/
            break;
        }
    }
    (*request)[strlen(*request)-1] = '\0'; /*removes new line*/
    *pid = strtol(*operation, endptr, 10);
    return check_op_req(*operation, *request, *endptr);
}

static void parse_args(int argc, char **argv){
    progname = argv[0];
    if(argc!=1 && argc!=3){
        usage();
    }
}

static bool check_op_req(char *operation, char *request, char *endptr){
    if(strcmp(operation, "sum")!=0 && strcmp(operation, "min")!=0 && strcmp(operation, "max")!=0 && strcmp(operation, "avg")!=0 && strcmp(endptr, "\0")!=0){
        (void)fprintf(stderr, "%s invalid request\n", progname);
        usage();
        return false;
    }
    if(strcmp(request, "cpu")!=0 && strcmp(request, "mem")!=0 && strcmp(request, "time")!=0 && strcmp(request, "command")!=0 && strcmp(request, "pid")!=0){
        (void)fprintf(stderr, "%s invalid request\n", progname);
        usage();
        return false;
    }
    if(strcmp(endptr, "\0")!=0 && strcmp(request, "command")==0){
        (void)fprintf(stderr, "%s invalid request\n", progname);
        usage();
        return false;
    }
    return true;
}

static void usage(){
    (void)fprintf(stderr, "%s usage:\n", progname);
    (void)fprintf(stderr, "%s [operation request]\n", progname);
    (void)fprintf(stderr, "%s operation: sum, min, max, avg, 'a valid pid'\n", progname);
    (void)fprintf(stderr, "%s request: cpu, mem, time, pid, command\n", progname);
    (void)fprintf(stderr, "%s \nnote that you can not call numeric operations on the request 'command'\n", progname);
    exit(EXIT_FAILURE);
}

static void setup_signal_handler(void){
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;

    s.sa_handler = signal_handler;
    s.sa_flags   = 0;
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