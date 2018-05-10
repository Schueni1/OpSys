/**
 * @file client.c
 * @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
 * @date 24.03.2017
 *
 * @brief This Program sends a coffee request (size, flavour) to the server(coffee machine)
    and prints the answer of the server on stdout.
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <strings.h>

#define BUFFER_BYTES (2)
#define RESPONSE_WIDTH (1)

static const char *progname; /* < Name of the program */

static int connfd = -1; /* < File descriptor for connection socket */

struct coffee{ /* < Struct storing information about the coffee request*/
    long int size;
    char *flavour;
};

struct opts{ /* < Struct storing information how to connect to the server*/
    char *host;
    char *port;
};

/**
 * @brief           Connect to the game server using given hostname and port.
 * @param host      the ip address or hostname of the server
 * @param port      the port the server is listening on
 * @return status   the status code of the connect() method
 * @details global variables: connfd: File descriptor for connection socket
 */
static int connect_to_server(char *host, char *port);

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 * @details global variables: progname.
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief Parse command line options and/or sets default values
 * @param argc The argument counter
 * @param argv The argument vector
 * @param options struct where parsed arguments host and port are stored
 * @param cof struct where parsed arguments size and flavour are stored
 */
static void parse_args(int argc, char **argv, struct opts *options, struct coffee *cof);

/**
 * @brief Sends and receives bytes to and from the server. Prints server answer on stdout
 * @param block contains coffee info flavour and size
 * @details global variable: progname
 */
static void communicate(uint16_t block);

/**
* @brief calculates the parity bit of buffer sent to the server.
* @param block contains flavour and size 
* @return the partity bit
*/
static uint8_t calculate_parity16(uint16_t block);

/**
* @brief calculates the parity bit of buffer recieved from the server.
* @param response contains time until coffee is produced or error information
* @return the partity bit
*/
static uint8_t calculate_parity8(uint8_t response);

/**
 * @brief Writes message to server
 * @param fd Socket to write from
 * @param buffer Buffer where wirte data is stored
 * @param n Size to write
 * @return number of bytes written
 */
static int write_to_server(int fd, uint8_t *buffer, size_t n);

/**
 * @brief Read message from socket (deals with partial reads)
 * @param fd socket to read from
 * @param buffer buffer where read data is stored
 * @param n size to read
 * @return -1 on failure number of bytes read on success
 */
static int read_from_server(int fd, uint8_t *buffer, size_t n);

/**
 * @brief free allocated resources (closes socket)
 */
static void free_resources(void);

/**
*Program entry point
*@brief calls the functions parse_args, connect_to_server and communicate. Checks if flavour exists.
        saves flavour and size into the uint16_t block.
*@param argc The argument counter
*@param argv The argument vector
*@details global variables: progname
*@return Returns EXIT_SUCCESS
*/
int main(int argc, char *argv[]){
    struct opts options;
    struct coffee cof;
    parse_args(argc, argv, &options, &cof);
    if (argc < 3){
      bail_out(EXIT_FAILURE,"[-h HOSTNAME] [-p PORT] SIZE FLAVOUR");
    }
    int flavourID = 255, size;
    static char *arr[] = {"Decaffeinato", "Kazaar", "Volluto", "Ciocattino",
     "Vanilio", "Linizio Lungo", "Vivalto Lungo","Fortissio Lungo",
     "Bukeela ka Ethiopia Lungo","Decaffeinato Lungo", "Cosi", "Capriccio", "Livanto",
     "Roma", "Arpeggio", "Ristretto", "Dharkan", "Dulsao do Brasil", "Rosabaya de Colombia",
     "Indrya from India", "Decaffeinato Intenso", "Caramelito", "Cauca", "Santander", "Cubania", 
     "Selection Vintage", "Sachertorte", "Linzer Torte", "Apfelstrudel", "SULUJA ti South Sudan", 
     "CAFECITO de Cuba", "Cafezinho do Brazil"};
    for(uint8_t i=0; i<sizeof(arr)/sizeof(char*); ++i){
        if(strcasecmp(arr[i], cof.flavour)==0){
            size = cof.size;
            flavourID = i;
            break;
        }
    }
    if(flavourID==255){
        bail_out(EXIT_FAILURE, "Flavour '%s' does not exist", cof.flavour);
    }
    cof.flavour = arr[flavourID];
    uint16_t block = flavourID;
    block += (size << 5);
    (void)printf("[%s] requestig a %ld ml cup of coffee of flavour '%s' (id=%d).\n", progname, cof.size, cof.flavour, flavourID);
    if (connect_to_server(options.host, options.port) < 0){
        bail_out(EXIT_FAILURE, "connection");
    }
    communicate(block);
    return EXIT_SUCCESS;
}

static void communicate(uint16_t block) {
    uint8_t read_buffer;
    uint8_t parity = calculate_parity16(block);
    block |= (parity << 15);
    uint8_t buff[2];
    buff[0] = block;
    buff[1] = block>>8;
    if(write_to_server(connfd, buff, BUFFER_BYTES) < 0) {
      bail_out(EXIT_FAILURE, "Error writing to server");
    }
    if(read_from_server(connfd, buff, RESPONSE_WIDTH) < 0) {
	    bail_out(EXIT_FAILURE, "Error reading from server");
    }
    read_buffer = buff[0]<<1;
    read_buffer = read_buffer>>1;
    uint8_t temp = read_buffer;
    parity = calculate_parity8(temp);
    if(parity != buff[0]>>7){
        bail_out(EXIT_FAILURE, "parity check failed");
    }
    if(read_buffer == 125){
        (void)fprintf(stderr, "[%s] Error %d - full_bin\n", progname, read_buffer>>5);
    }
    else if(read_buffer == 126){
        (void)fprintf(stderr, "[%s] Error %d - no_water\n", progname, read_buffer>>5);
    }
    else if(read_buffer == 127){
        (void)fprintf(stderr, "[%s] Error %d - no_water_and_full_bin\n", progname, read_buffer>>5);
    }
    else{
        (void)printf("[%s] Coffee ready in %ds.\n",progname, read_buffer);
    }
}

static uint8_t calculate_parity8(uint8_t block){
  int8_t parity_calc = 0;
  for (int i = 0; i < 7; ++i) {
    parity_calc ^= block;
    block >>= 0x1;
  }
  return parity_calc &= 0x1;
}

static uint8_t calculate_parity16(uint16_t block){
  int8_t parity_calc = 0;
  for (int i = 0; i < 15; ++i) {
    parity_calc ^= block;
    block >>= 0x1;
  }
  return parity_calc &= 0x1;
}

static int connect_to_server(char *host, char *port) {

    struct addrinfo hints;
    struct addrinfo *result;

    int addr, conn;
    bzero(&hints, sizeof(hints));

    hints.ai_family = AF_UNSPEC;          //IPv4
    hints.ai_socktype = SOCK_STREAM;    //TCP
    addr = getaddrinfo(host, port, &hints, &result);

    if(addr!=0){
      bail_out(EXIT_FAILURE, "getaddrinfo: %s\n", gai_strerror(addr));
    }
    //create socket file descriptor
    connfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(connfd == -1){
      bail_out(EXIT_FAILURE, "socket()");
    }

    /* Connect the socket to the address */
    conn = connect(connfd, result->ai_addr, result->ai_addrlen);
    if(conn == -1){
      bail_out(EXIT_FAILURE, "connect");
    }
    freeaddrinfo(result);

    return conn;
}

static void bail_out(int exitcode, const char *fmt, ...) {

    va_list ap;

    (void) fprintf(stderr, "%s: ", progname);
    if (fmt != NULL) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");

    free_resources();
    exit(exitcode);
}

static void parse_args(int argc, char **argv, struct opts *options, struct coffee *cof)
{
    char *endptr;
    progname = argv[0];
    options->host = "localhost";
    options->port = "1821";
    int argument;
    while((argument = getopt(argc, argv, "h:p:"))!=-1){
        switch(argument){
            case 'h': 
                options->host = optarg;
                break;
            case 'p': 
                options->port = optarg;
                break;
            case '?': 
                bail_out(EXIT_FAILURE, "[-h HOSTNAME] [-p PORT] SIZE FLAVOUR");
            default: 
                assert(0); //unreachable
        }
    }
    cof->size = strtol(argv[argc-2], &endptr, 10);
    cof->flavour = argv[argc-1];
}

static int write_to_server(int fd, uint8_t *buffer, size_t n){

	size_t bytes_sent = 0;

	do {
		ssize_t r;
		r = write(fd, buffer + bytes_sent, n - bytes_sent);
		if (r <= 0) {
			bail_out(EXIT_FAILURE, "write");
		}
		bytes_sent += r;
	} while (bytes_sent < n);

	if (bytes_sent < n) {
		bail_out(EXIT_FAILURE, "write");
	}

	return bytes_sent;
}

static int read_from_server(int fd, uint8_t *buffer, size_t n)
{

	size_t bytes_read = 0;

	do {
		ssize_t r;
		r = read(fd, buffer + bytes_read, n - bytes_read);
		if (r <= 0) {
			return -1;
		}
		bytes_read += r;
	} while (bytes_read < n);

	if (bytes_read < n) {
		return -1;
	}

	return bytes_read;
}

static void free_resources(void) {

    /* clean up resources */
    if (connfd >= 0) {
        (void) close(connfd);
    }
}