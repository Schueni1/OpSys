/**
 * @file server.c
 * @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
 * @date 24.03.2017
 *
 * @brief This Program reads the clients coffee request, prints the list of coffees in the queue and answers the client.
            The answer is either the time it will take to produce its  coffee request por an error and it's cause (no_water, full_bin, no_water_and_full_bin).
 **/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

/* Length of an array */
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))
#define BUFFER_BYTES (2)
#define READ_BYTES (2)
#define BACKLOG (5)

/* @brief Name of the program */
static const char *progname;    

 /* @brief File descriptor for server socket */
static int sockfd = -1;   

/* @brief File descriptor for connection socket */
static int connfd = -1;    

/* @brief This variable is set upon receipt of a signal */
volatile sig_atomic_t quit = 0;     

/* === Type Definitions === */

/* @brief struct containing info parsed by argument as well as request recieved from client */
struct opts {  
    char *portno;
    long int water;
    long int binsize;
    int cups;
};

/* @brief Linkedlist node */
struct node { 
    uint16_t size;
    uint8_t flavourID;
    struct node *next;
}typedef node_t;

/* @brief head of the Linkedlist*/
static node_t *head = NULL; 

/**
 * @brief Signal handler
 * @param sig Signal number catched
 */
static void signal_handler(int sig);

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 * @brief global variable: progname
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief closes socket and conn to client. Calls function free_list.
 */
static void free_resources(void);

/**
* @brief free memory allocated by the linkedlist
* @detail global variables: sockfd, connfd
*/
static void free_list(void);

/**
 * @brief Parse command line options and/or sets default values. Prints initial status
 * @param argc The argument counter
 * @param argv The argument vector
 * @param options Struct where parsed arguments are stored
 * @detail global variable: progname
 */
static void parse_args(int argc, char **argv, struct opts *options);

/**
 * @brief Read message from socket
 * @param sockfd_con Socket to read from
 * @param buffer Buffer where read data is stored
 * @param n Size to read
 * @return Pointer to buffer on success, else NULL
 */
static uint8_t *read_from_client(int fd, uint8_t *buffer, size_t n);

/**
 * @brief Writes message to client
 *
 * @param sockfd_con Socket to write from
 * @param buffer Buffer where wirte data is stored
 * @param n Size to write
 * @return number of bytes written
 */
static int write_to_client(int fd, uint8_t *buffer, size_t n);

/**
* @brief calculates the parity bit of buffer recieved from client.
* @param block contains flavour and size 
* @return the partity bit
*/
static uint8_t calculate_parity16(uint16_t info);

/**
* @brief checks if an error accured. Updates water and cups. Prints info about the cup request. Calls function push_coffe_queue
* @param buff contains request sent by client.
* @param machine contains info about water and cups
* @details global variable: progname 
* @return Returns the answer for client without parity bit. (either time or error code);
*/
static uint8_t create_coffee(uint8_t *buff, struct opts *machine);

/**
* @brief adds and removes coffees from the linkedlist and calculates the time to prodece the clients request. Calls the function print_list
* @param size coffee request
* @param flavourID coffee request
* @param arr contains the names of all flavours
* @param water_bin_status if true coffee is pushed to the queue
* @details global variables: progname, head 
* @return Returns the time to produce the clients request
*/
static int push_coffee_queue(uint16_t size, uint8_t flavourID, char *arr[], bool water_bin_status);

/**
* @brief calculates the parity bit of buffer sent to client.
* @param response contains time until coffee is produced or error information
* @return the partity bit
*/
static uint8_t calculate_parity8(uint8_t info);

/**
* @brief creates new TCP/IP socket, set the SO_REUSEADDR
       option for this socket, bind the socket to localhost:portno,
       listen, and wait for new connections, which should be assigned to
       `connfd`. Calls function 'communicate'. Terminates in case of error.
* @param options contains parsed arguments
*/
static void setup(struct opts *options);

/**
* @brief accepts the connection to the client. Calls functions read_from_client. Calls function write_to_client with return value of create_coffee.
    compares parity bits. Calls function calculate_parity8 and calculate_parity16. Closes connection to client.
* @param options contains info parsed by argument
* @detail global variables: progname (Program name), connfd (connection socket)
*/
static void communicate(struct opts *options);

/**
* @brief pushes new coffee to linkedlist.
* @param size ml of coffee request
* @param flavourID of coffee request
* @details global variable: head
*/
static void push(uint16_t size, uint8_t flavourID);

/**
* @brief pops the first coffee (head) from the list and returns its size
* @details global variable: head
* @return size of head
*/
static int pop(void);

/*
* @brief returns head->size ml
* @details global variable: head
* @return head->size
*/
static int peak(void);

/**
* @brief returns size sum of all cups in the linkedlist
* @details global variable: head
* @return Returns size sum of all cups in the linkedlist
*/
static int sum_list(void);

/**
* @brief prints a message on stdout if linkedlist is empty, else prints all elements in the linked list (size, id, flavour).
* @param arr contains the names of all coffee flavours to match the ID
* @detail global variables: head, progname
*/
static void print_list(char *arr[]);

/**
*Program entry point
*@brief saves program name, sets up signal handler, calls function parse_args, setup, free_resources.
*@param argc The argument counter
*@param argv The argument vector
*@details global variables: progname
*@return Returns EXIT_SUCCESS
*/
int main(int argc, char *argv[]){   
    progname = argv[0];
/* setup signal handlers */
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
    struct opts options;
    parse_args(argc, argv, &options);
    setup(&options);  
    free_resources();
    printf("test\n");
    return EXIT_SUCCESS;
}

static void setup(struct opts *options){

    struct addrinfo hints;
    struct addrinfo *ai;
    int res;
    int reuse = 1;
    (void)memset (&hints , 0, sizeof (hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* <-- */
    res = getaddrinfo (NULL, options->portno, &hints , &ai);
    if(res!=0){
        bail_out(EXIT_FAILURE, "getaddrinfo");
    }
    sockfd = socket (ai->ai_family , ai->ai_socktype, ai->ai_protocol);
    if(sockfd==-1){
        bail_out(EXIT_FAILURE, "socket");
    }
    if((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) < 0){
        bail_out(EXIT_FAILURE, "setsockopt");
    }
    /* Assign the address to the socket */
    res = bind (sockfd, ai->ai_addr, ai->ai_addrlen);
    if(res==-1){
        bail_out(EXIT_FAILURE, "bind");      
    }
    if((listen(sockfd, BACKLOG)) < 0){
        bail_out(EXIT_FAILURE, "listen");     
    }
    freeaddrinfo (ai);
    while(1){
        communicate(options);
        //printf("sleep 20\n");
        //sleep(20);
    }
}

static void communicate(struct opts *options){
    (void)printf("[%s] Waiting for client ...\n", progname);
    if((connfd = accept(sockfd, NULL, NULL)) < 0){
        bail_out(EXIT_FAILURE, "accept");   
    }
    else{
        (void)printf("[%s] Client connected.\n", progname);
    }
    static uint8_t buffer[BUFFER_BYTES];

    /* read from client and save to buffer */
    if (read_from_client(connfd, &buffer[0], READ_BYTES) == NULL){
        bail_out(EXIT_FAILURE, "read_from_client");
    }
    /*check parity*/
    uint16_t block = (buffer[1]<<8) + buffer[0];
    uint8_t parity = calculate_parity16(block);
    if(parity!=(block>>15)){
        bail_out(EXIT_FAILURE, "parity error");
    }
    
    buffer[0] = create_coffee(buffer, options);
    parity = calculate_parity8(buffer[0]);
    buffer[0] |= (parity<<7);
    if(write_to_client(connfd, &buffer[0], sizeof(buffer[0])) < 0){
          bail_out(EXIT_FAILURE, "send");
    }
    (void)printf("[%s] Close connection to client.\n", progname);

    close(connfd);
}

static uint8_t create_coffee(uint8_t *buffer, struct opts *machine){
    static char *arr[] = {"Decaffeinato", "Kazaar", "Volluto", "Ciocattino",
     "Vanilio", "Linizio Lungo", "Vivalto Lungo","Fortissio Lungo",
     "Bukeela ka Ethiopia Lungo","Decaffeinato Lungo", "Cosi", "Capriccio", "Livanto",
     "Roma", "Arpeggio", "Ristretto", "Dharkan", "Dulsao do Brasil", "Rosabaya de Colombia",
     "Indrya from India", "Decaffeinato Intenso", "Caramelito", "Cauca", "Santander", "Cubania", 
     "Selection Vintage", "Sachertorte", "Linzer Torte", "Apfelstrudel", "SULUJA ti South Sudan", 
     "CAFECITO de Cuba", "Cafezinho do Brazil"};
    uint8_t flavourID = buffer[0]<<3;
    flavourID = flavourID>>3;
    uint8_t temp_no_parity = buffer[1]<<1;
    buffer[1] = temp_no_parity>>1;
    uint16_t size = ((buffer[0]>>5) + (buffer[1]<<3));
    buffer[0] = 0x0;
    if(machine->cups <= machine->binsize){
        ++machine->cups;
    }
    if(machine->binsize < machine->cups){
        buffer[0] = 1;
        for(int i = 6; i>1; --i){
            buffer[0] |= (1<<i);
        }
    }
    if(machine->water < size){
        buffer[0] += 2;
        for(int i = 6; i>1; --i){
            buffer[0] |= (1<<i);
        }
    }
    if(buffer[0] > 124){
        (void)push_coffee_queue(size, flavourID, arr, false);
    }
    if(buffer[0] == 125){
        (void)fprintf(stderr, "[%s] Error - full_bin.\n", progname);
    }
    else if(buffer[0] == 126){
        (void)fprintf(stderr, "[%s] Error - no_water.\n", progname);
    }
    else if(buffer[0] == 127){
        (void)fprintf(stderr, "[%s] Error - no_water_and_full_bin.\n", progname);
    }
    else{
        machine->water-=size;
        (void)printf("[%s] New status: %ldml, %d cups bin.\n", progname, machine->water, machine->cups);
        int all_time = push_coffee_queue(size, flavourID, arr, true);
        (void)printf("[%s] Finish in %ds.\n", progname, all_time);
        (void)printf("[%s] Start coffee for %dml cup with flavour '%s'.\n", progname, size, arr[flavourID]);
        buffer[0] = all_time;
    }
    return buffer[0];
}

static int push_coffee_queue(uint16_t size, uint8_t flavourID, char *arr[], bool water_bin_status){
    static time_t start = 0, end = 0;
    if(water_bin_status){
        push(size, flavourID);
    }
    static int all_time = 0;
    if(time(&end)==-1){
        bail_out(EXIT_FAILURE, "Error time() call end failed");
    }
    if(all_time!=0){
        all_time -= difftime (end,start);
        if(all_time<0){
        all_time = 0;
        }
    }
    if(time(&start)==-1){
        bail_out(EXIT_FAILURE, "Error time() call start failed");
    }
    if(water_bin_status){
        if(all_time<=0){
             all_time = size/10;
        }
        else{
          all_time += size/10;
        }
        if(all_time != size/10){
          (void)printf("[%s] Another coffee still in production.\n", progname);
        }
    }
    int sum = (sum_list()/10);
    while(head!=NULL && sum-(peak()/10) >= all_time){
        sum -= (pop()/10);
    }
    print_list(arr);
    return all_time;
}

static uint8_t calculate_parity8(uint8_t info){
  int8_t parity_calc = 0;
  for (int i = 0; i < 7; ++i) {
    parity_calc ^= info;
    info >>= 0x1;
  }
  return parity_calc &= 0x1;
}


static uint8_t calculate_parity16(uint16_t info){
  int8_t parity_calc = 0;
  for (int i = 0; i < 15; ++i) {
    parity_calc ^= info;
    info >>= 0x1;
  }
  return parity_calc &= 0x1;
}

static uint8_t *read_from_client(int fd, uint8_t *buffer, size_t n){
    /* loop, as packet can arrive in several partial reads */
    size_t bytes_recv = 0;
    do {
        ssize_t r;
        r = recv(fd, buffer + bytes_recv, n - bytes_recv, 0);
        if (r <= 0) {
            return NULL;
        }
        bytes_recv += r;
    } while (bytes_recv < n);

    if (bytes_recv < n) {
        return NULL;
    }
    return buffer;
}

static int write_to_client(int fd, uint8_t *buffer, size_t n){

	size_t bytes_sent = 0;
	do {
		ssize_t r;
		r = write(fd, buffer + bytes_sent, n - bytes_sent);
		if (r <= 0) {
			return -1;
		}
		bytes_sent += r;
	} while (bytes_sent < n);

	if (bytes_sent < n) {
		return -1;
	}
	return bytes_sent;
}

static void signal_handler(int sig)
{
    free_resources();
    quit = 1;
    exit(quit);
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
    exit(exitcode);
}

static void free_resources(void)
{
    /* clean up resources */
    if(connfd >= 0) {
        (void) close(connfd);
    }
    if(sockfd >= 0) {
        (void) close(sockfd);
    }
    free_list();
}
static void free_list(void){
    node_t * current = head;
    node_t * prev;
    while (current != NULL) {
        prev = current;
        current = current->next;
        free(prev);
    }
}

static void parse_args(int argc, char *argv[], struct opts *options)
{
    char *endptr;
    progname = argv[0];
    options->portno = "1821";
    options->water = 1000;
    options->binsize = 10;
    options->cups = 0;
    int pcount, lcount, ccount;
    pcount = lcount = ccount = 0;
    int argument;
    while ((argument = getopt (argc, argv, "p:l:c:")) != -1){
        switch(argument){
            case 'p':
                if(pcount==0){
                    options->portno = optarg;
                }
                else{
                    (void)fprintf(stderr, "Multiple occurrences of argument -p.\n");
                    exit(EXIT_FAILURE);
                }
                ++pcount;
                break;
            case 'l': 
                 if(lcount==0){
                    options->water = strtol(optarg, &endptr, 10);
                 }
                 else{
                    (void)fprintf(stderr, "Multiple occurrences of argument -l.\n");
                    exit(EXIT_FAILURE);
                  }
                  ++lcount;
                 break;
            case 'c': 
                 if(ccount==0){
                    options->binsize = strtol(optarg, &endptr, 10);
                 }
                 else{
                    (void)fprintf(stderr, "Multiple occurrences of argument -c.\n");
                    exit(EXIT_FAILURE);
                 }
                 ++ccount;
                break;
            case '?': 
                bail_out(EXIT_FAILURE, "");
            default: 
                assert(0); //unreachable
        }
    }
    (void)printf("[%s] Initialstatus: %ld ml water , %ld cups bin\n",progname ,options->water, options->binsize);
}

static void push(uint16_t size, uint8_t flavourID){
    if(head==NULL){
        head = malloc(sizeof(node_t));
        if(head==NULL){
            bail_out(EXIT_FAILURE, "Error malloc head failed");
        }
        head->size = size;
        head->flavourID = flavourID;
        head->next = NULL;
    }
    else{
        node_t * current = head;
        while (current->next != NULL) {
            current = current->next;
        }
        /* now we can add a new variable */
        current->next = malloc(sizeof(node_t));
        current->next->size = size;
        current->next->flavourID = flavourID;
        current->next->next = NULL;
    }
}

static int pop(void){
    int size = -1;
    node_t * next_node = NULL;
    if (head == NULL) {
        return -1;
    }
    next_node = head->next;
    size = head->size;
    free(head);
    head = next_node;
    return size;
}

static int peak(void){
    return head->size;
}

static int sum_list(void){
    node_t * current = head;
    int sum = 0;
    while (current != NULL) {
        sum += current->size;
        current = current->next;
    }
    return sum;
}

static void print_list(char *arr[]){
    node_t * current = head;
    if(current==NULL){
        (void)printf("[%s] List is empty\n", progname);
    }
    else{
        (void)printf("[%s] List elements:\n", progname);
    }
    while (current != NULL) {
        (void)printf("[%s] %dml cup of coffee with flavour '%s' (id=%d).\n",progname, current->size, arr[current->flavourID], current->flavourID);
        current = current->next;
    }
}