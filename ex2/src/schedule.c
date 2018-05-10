/**
 * @file schedule.c
 * @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
 * @date 16.04.2017
 *
 * @brief This Program repeatedly executes a program parsed by command line argument as it's grandchild. The interval
 within the program should be executed can also be set as command line argument. If the program fails, an emergency program,
 provided by the user, is executed once. All output of the users program and emergency program is written on stdout.
 All output of the users program on stdout is written to the File. 
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

/* @brief Length of an array*/
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0])) 

/**
* struct containing info parsed by argument
*/
struct arguments{           
    int intervall;
    int offset;
    char *program;
    char *emergency;
    char *logfile;
};

/**
* This variable is set upon receipt of a signal 
*/
volatile sig_atomic_t quit = 0; 

/**
* File pointer to file parsed by argument
*/
static FILE *f;                         

/** 
* Name of the program 
*/
static const char *progname;            

/**
 * @brief Parse command line options and/or sets default values. Calls usage if arguments parsed are incorrect.
 * @param argc The argument counter
 * @param argv The argument vector
 * @detail global variable: progname
 * @return on success returns struct arguments containing all information parsed by command line
 */
static struct arguments parse_args(int argc, char **argv);

/**
* @brief prints usage on stderr. Exits program with EXIT_FAILURE
* @details global variable: progname
*/
static void usage();

/**
 * @brief terminate program on program error. Prints info about the error.
 * @param exitcode exit code
 * @param fmt format string
 * @details global variable: progname
 * @brief global variable: progname
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
* @brief closes the file provided by the user.
* @details gloabal variable: f pointer to file parsed by argument.
*/
static void free_resources(void);

/**
* @brief returns time until next execution of program
* @param interval: minimum wait until execution of program
* @param offset: offset+interval is maximum wait until execution of program
* @return random wait time between intervall and intervall+offset
*/
static int getRandExTime(int intervall, int offset);

/**
* @brief creates a Child process and calls function create_grand_child. Reads from pipe p, prints output to stdout and writes
to File provided by user.
* @param args: struct containing info parsed by argument to be passed to create_grand_child.
* @detail global variable f: pointer to File parsed by argument and needed to write to file.
    global variable: progname.
* @details global variable quit, to check whether a signal was recieved.
* @return status of child process. 0 on success >0 on error.
*/
static int create_Child(struct arguments args);

/**
* @brief executes "program" as grandchild in a loop and writes output of stdout to the parents pipe. "program" is executed, 
as long as it's exitcode is "EXIT_SUCCESS". Otherwise "emergency" is executed once as a child.
* @param args struct containing info parsed by argument. (program, emergency, intervall, offset are needed in this function)
* @param parent_p is the parents pipe. In the child process the content from the grandchild is written to the parent pipe.
* @details global variable quit, to check whether a signal was recieved.
*/
static void create_grand_child(struct arguments args, int parent_p[]);

/**
* @brief sets up signal handler with SIGINT and SIGTERM
*/
static void setup_signal_handler(void);

/**
* @brief is executed on recieve of SIGINT or SIGTERM signal. 
* @param sig Signal number catched.
* @details global variable quit is set to signal id.
*/
static void signal_handler(int sig);

/**
*Program entry point
*@brief calls function parse_args, setup_signal_handler, create_Child, free_resources. Writes either "EMERGENCY SUCCESFULL" or
"EMERGENCY FAILURE" to stdout and to the File parsed by command line. If a signal was caught, the signal ID is printed on stdout.
*@param argc The argument counter
*@param argv The argument vector
*@details global variable quit, to check whether a signal was recieved.
*@return Returns EXIT_SUCCESS if create_Child call exits with exitcode EXIT_SUCCESS.
*/
int main(int argc, char **argv){
    setup_signal_handler();
    const struct arguments args = parse_args(argc, argv);
    int ret = create_Child(args);
    if(ret==EXIT_SUCCESS && quit==0){
        (void)printf("EMERGENCY SUCCESFULL\n");
        if(fprintf(f, "EMERGENCY SUCCESFULL\n")<0){
             bail_out(EXIT_FAILURE, "write to file error");
        }
    }
    else if (WIFSIGNALED(quit)){
        (void)printf("\nProcess killed (signal %d)\n", WTERMSIG(quit));
    } 
    else if (WIFSTOPPED(quit)){
         (void)printf("\nProcess stopped (signal %d)\n", WSTOPSIG(quit));
    }
    else {
        (void)printf("EMERGENCY FAILURE\n");
        if(fprintf(f, "EMERGENCY FAILURE\n")<0){
             bail_out(EXIT_FAILURE, "write to file error");
        }
        free_resources();
        exit(EXIT_FAILURE);
    }
    free_resources();
    return EXIT_SUCCESS;
}

static int getRandExTime(int intervall, int offset){
    void *p;
    time_t seed = time(NULL);
    p = &seed;
    long int temp = (long int)p;
    int i = (int) temp % 10000000;
    if(seed == (time_t) - 1){
        (void)fprintf(stderr, "%s time call failed\n", progname);
        exit(EXIT_FAILURE);
    }
    srand(seed - i);
    if(intervall==0){

    }
    return rand()%(offset+1)+intervall;
}

static int create_Child(struct arguments args){
    f = fopen(args.logfile, "w");
    if(f==NULL){
        bail_out(EXIT_FAILURE, "open file fail");
    }
    int p[2], pid;
    int status;
    /* open pipe */
    if(pipe(p) == -1){
        bail_out(EXIT_FAILURE, "open pipe fail");
    }
    switch(pid = fork()){
        case -1:
         bail_out(EXIT_FAILURE, "fork call fail");

        case 0:  /* if child then write down pipe */
            create_grand_child(args, p);

        default:   /* parent reads pipe */
            if(close(p[1])<0){ /* first close the write end of the pipe */
                bail_out(EXIT_FAILURE, "closing write end fail");
            }
            
            char buff[1024];
            bzero(buff, sizeof(buff));
            while(read(p[0], buff, 1022)>0){
                (void)printf("%s", buff); //printing message to stdout
                if(fprintf(f,"%s", buff)<0){ //writing to logfile
                    bail_out(EXIT_FAILURE, "write error fail");
                }
                bzero(buff, sizeof(buff));
            }
            if(wait(&status)<0){
                (void)fprintf(stderr, "%s wait fail\n", progname);
            }
            if (WIFSIGNALED(status)){
                bail_out(EXIT_FAILURE, "Child killed (signal %d)", WTERMSIG(status));
            } 
            else if (WIFSTOPPED(status)){
                bail_out(EXIT_FAILURE, "Child stopped (signal %d)\n", WSTOPSIG(status));
            }
       }
       return status;
}

static void create_grand_child(struct arguments args, int parent_p[]){
  int p[2], pid;
  setup_signal_handler();
  int status;
    /* open pipe */
  do{
    if(pipe(p) == -1){
       bail_out(EXIT_FAILURE, "open pipe fail");
    }
     switch(pid = fork()){
        case -1:
         bail_out(EXIT_FAILURE, "fork call fail");

        case 0:  /* if grandchild then write down pipe */
            if(close(p[0])<0){   //first close the read end of the pipe 
                bail_out(EXIT_FAILURE, "fork call fail");
            }
            if(dup2(p[1], STDOUT_FILENO) == -1){ /* stdout == write end of the pipe */
               bail_out(EXIT_FAILURE, "dup2 fail");
            }
            if(close(p[1])<0){
                bail_out(EXIT_FAILURE, "close write end fail");
            }
            char* argz[] ={args.program, NULL};
            execv(argz[0], argz);
            bail_out(EXIT_FAILURE, "execv %s fail", args.program);
        default:   /* child reads pipe */
            if(close(p[1])<0){ /* first close the write end of the pipe */
                bail_out(EXIT_FAILURE, "closing write end fail");
            }
            char buffer[1024];
            bzero(buffer, sizeof(buffer));
            if(read(p[0], buffer, 1024)<0){
                bail_out(EXIT_FAILURE, "read from pipe");
            }
            if(write(parent_p[1], buffer, strlen(buffer)+1)<0){
                bail_out(EXIT_FAILURE, "write to parent_pipe");
            }
            if(wait(&status)<0){
                (void)fprintf(stderr, "%s wait fail\n", progname);
            }
            else if (WIFSIGNALED(status)){
                bail_out(EXIT_FAILURE, "Child killed (signal %d)\n", WTERMSIG(status));
            } 
            else if (WIFSTOPPED(status)){
                bail_out(EXIT_FAILURE,"Child stopped (signal %d)\n", WSTOPSIG(status));
            }
            if(status==0 && quit==0){
                sleep(getRandExTime(args.intervall, args.offset));
            }
            else if(quit==0){
                char* argz[] ={args.emergency, NULL};
                execv(argz[0], argz);
                bail_out(EXIT_FAILURE, "execv %s fail", args.emergency);
            }
            else{
                exit(status);
            }
     }
    }
    while(status==0 && quit==0);
    if(quit!=0){
        exit(status);
    }
 }


static struct arguments parse_args(int argc, char **argv){
    progname = argv[0];
    if(argc<4 || argc%2!=0 || argc > 8){
        usage();
    }
    struct arguments args;
    args.intervall = 1;
    args.offset = 0;
    args.program = NULL;
    args.emergency = NULL;
    args.logfile = NULL;
    char *endptr;
    int opt;
    bool opt_s = false;
    bool opt_f = false;
    while((opt = getopt(argc, argv, "s:f:"))!=-1){
        switch(opt){
            case 's':
                if(!opt_s){
                    args.intervall = (int) strtol(optarg, &endptr, 10);
                    if((strcmp(endptr, "\0")!=0)){
                        usage();
                    }
                    opt_s = true;
                }
                else{
                    usage();
                }
                break;
            case 'f':
                if(!opt_f){
                    args.offset = (int) strtol(optarg, &endptr, 10);
                    if((strcmp(endptr, "\0")!=0)){
                        usage();
                    }
                    opt_f = true;
                }
                else{
                    usage();
                }
                break;
            case '?': 
                usage();
                break;
            default: 
                assert(0);
        }
    }

    if(opt_s && opt_f){
        args.logfile = argv[7];
        args.emergency = argv[6];
        args.program = argv[5];
    }
    else if(opt_s || opt_f){
        args.logfile = argv[5];
        args.emergency = argv[4];
        args.program = argv[3];
    }
    else{
        args.logfile = argv[3];
        args.emergency = argv[2];
        args.program = argv[1];
    }
    if(args.logfile==NULL || args.program==NULL || args.emergency==NULL){
        usage();
    }
    return args;
}

static void usage(){
    (void)fprintf(stderr, "%s [-s <seconds>] [-f <seconds>] <program> <emergency> <logfile>\n", progname);
    exit(EXIT_FAILURE);
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;
    if(strcmp(fmt, "")!=0 || errno!=0){
        (void) fprintf(stderr, "%s: ", progname);
    }
    if (fmt != NULL){
         va_start(ap, fmt);
         if(strcmp(fmt, "")!=0){
            (void) vfprintf(stderr, fmt, ap);
         }
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
     if(strcmp(fmt, "")!=0){
        (void) fprintf(stderr, "\n");
     }
    free_resources();
    _exit(exitcode);
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
        if (sigaction(signals[i], &s, NULL) < 0) {
            (void)fprintf(stderr, "%s sigfillset", progname);
            exit(EXIT_FAILURE);
        }
    }
}

static void signal_handler(int sig)
{    
    quit = sig;
}

static void free_resources(void){
    /* clean up resources */
    if(f!=NULL){
        fclose(f);
    }
}