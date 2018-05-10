/**
 * @file client.c
 *
 * @author  Aaron Duxler, #1427540
 * @brief   Connects to a server and generates mastermind guesses.
 * @date    2016-10-23
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>

#define PARITY_ERR_BIT (6)
#define SHIFT_WIDTH (3)
#define MAX_TRIES (35)
#define SLOTS (5)
#define COLORS (8)
#define COMBINATIONS (32768)

#define BUFFER_BYTES (2)
#define RESPONSE_WIDTH (1)
#define EXIT_PARITY_ERROR (2)
#define EXIT_GAME_LOST (3)
#define EXIT_MULTIPLE_ERRORS (4)

#define BACKLOG (5)


/* === Macros === */

#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif


/* === Global Variables === */

/*stores all possible Combinations*/
uint16_t allCombinations[COMBINATIONS];

/* Name of the program */
static const char *progname = "client";

/* File descriptor for connection socket */
static int connfd = -1;


/* === Prototypes === */


/**
 * Connects to a given server and port.
 *
 * @brief           Connect to the game server using given hostname and port.
 * @param host      the ip address or hostname of the server
 * @param port      the port the server is listening on
 * @return status   the status code of the connect() method
 */
static int connect_to_server(char *host, char *port);

/**
 *
 * @brief Sends and receives bytes to and from the server
 */
static int communicate(void);

/**
 * @brief Selects one of the not yet excluded combinations in the array.
    selects the first combination with 4 different colours
    or if there is no such combination selects the last combination with the most different coulors.
 * @return two bytes containing the color information and a parity bit
 */
static uint16_t compute_patern(void);

/**
  * @brief Writes all combinations in the global array allCombinations
*/
static void compute_all_combinations(void);

/**
  *@brief exludes combinations by comparing the previous guess with all combinations left
    and the answer of the previous guess.
  *@param redWhiteBuff contains the number of red and white for the previous guess
  *@param prev_guess is the previous guess as an array
  *@return number of remaining combinations
*/
static int exclude_combinations(uint8_t redWhiteBuff, uint8_t *prev_guess);

/**
 * @brief Compute answer to request
 * @param req Client's next guess
 * @param resp Buffer will contain the answer for the guess req
 * @param secret is the the prev_guess of client
 * @return Number of correct matches on success
 */
static int check_answer(uint16_t req, uint8_t *resp, uint8_t *secret);

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
* @brief calculates the parity bit for the combination
* @param selected_colors combination to calculate parity bit
* @return the partity bit
*/
static uint8_t calculate_parity(uint16_t selected_colors);

/**
 * @brief free allocated resources (closes socket)
 */
static void free_resources(void);

/**
 * @brief Writes message to server
 *
 * @param sockfd_con Socket to write from
 * @param buffer Buffer where wirte data is stored
 * @param n Size to write
 * @return number of bytes written
 */
static size_t write_to_server(int fd, uint8_t *buffer, size_t n);

/**
 * @brief Read message from socket (deals with partial reads)
 * @param sockfd_con Socket to read from
 * @param buffer Buffer where read data is stored
 * @param n Size to read
 * @return Pointer to buffer on success, else NULL
 */
static size_t read_from_server(int fd, uint8_t *buffer, size_t n);


/* === Implementations === */

/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vector
 * @return EXIT_SUCCESS on success, EXIT_PARITY_ERROR in case of an parity
 * error, EXIT_GAME_LOST in case client needed to many guesses,
 * EXIT_MULTIPLE_ERRORS in case multiple errors occured in one round
 */
int main(int argc, char **argv) {

    if (argc != 3){
      bail_out(EXIT_FAILURE,"Usage: %s <hostname> <server-port>", progname);
    }
    int ret = EXIT_SUCCESS;

    if (connect_to_server(argv[1], argv[2]) < 0){
        bail_out(EXIT_FAILURE, "connection");
      }
    else {
        ret = communicate();
        if(ret < 0){
          bail_out(EXIT_FAILURE, "communicate");
        }
      }
    free_resources();
    return ret;
}

static int connect_to_server(char *host, char *port) {

    struct addrinfo hints;
    struct addrinfo *result;

    int addr, conn;
    bzero(&hints, sizeof hints);

    hints.ai_family = AF_INET;          //IPv4
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
      bail_out(EXIT_FAILURE, "connect()");
    }
    freeaddrinfo(result);

    return conn;
}

static int communicate(void) {
    int ret = EXIT_SUCCESS;
    uint8_t read_buffer;
    uint16_t next_try;

    compute_all_combinations();

    for (int i = 0; i < MAX_TRIES; i++) {
        int error = 0;
        next_try = compute_patern();
        uint8_t buff[BUFFER_BYTES];
        uint16_t value = next_try;
        buff[0] = value & 0xff;
        buff[1] = (value >> 8) & 0xff;
        if(write_to_server(connfd, buff, BUFFER_BYTES) < 0) {
		        bail_out(EXIT_FAILURE, "Error writing to server");
          }

        DEBUG("Sent 0x%x\n", next_try);

        if(read_from_server(connfd, buff, RESPONSE_WIDTH) < 0) {
		      bail_out(EXIT_FAILURE, "Error reading from server");
        }

        read_buffer = buff[0];
        DEBUG("Got byte 0x%x\n", read_buffer);

        // check for errors in received buffer
        switch (read_buffer >> 6) {
              //linux.die.net/man/2/recv
            case 0:       //All ok
                break;
            case 1:
                (void)printf("%s\n", "Parity error");
                error = 1;
                ret = EXIT_PARITY_ERROR;
                break;

            case 2:
                (void)printf("%s\n", "Game lost");
                error = 1;
                ret = EXIT_GAME_LOST;
                break;

            case 3:
                (void)printf("%s\n", "Multiple errors");
                error = 1;
                ret = EXIT_MULTIPLE_ERRORS;
                break;

            default:
                assert(0); //Unreachable
        }
        if(error){
          free_resources();
          exit(ret);
        }
        // correct combination found
        else if ((read_buffer & 7) == 5) {
            printf("Runden: %d\n", i + 1);
            return ret;
        }
        else{
          uint8_t prev_try[SLOTS];
          value = next_try;
          buff[0] = value & 0xff;
          buff[1] = (value >> 8) & 0xff;
          for (int j = 0; j < SLOTS; ++j) {
              int tmp = next_try & 0x7;
              prev_try[j] = tmp;
              next_try >>= SHIFT_WIDTH;
          }

          int comb = exclude_combinations(read_buffer, prev_try);
          assert(comb > 0); //its impossible, that there are less combinations remaining than 0
          DEBUG("remaining comb: %d\n", comb);
        }
    }
    return ret;
}

static void compute_all_combinations(void) {
  for (uint16_t i = 0; i < COMBINATIONS; i++) {
    allCombinations[i] = i;
  }
}


static int check_answer(uint16_t req, uint8_t *resp, uint8_t *secret){
    int colors_left[COLORS];
    int guess[COLORS];
    int red_temp, white_temp;
    int j;

    /* extract the guess  */
    for (j = 0; j < SLOTS; ++j) {
        int tmp = req & 0x7;
        guess[j] = tmp;
        req >>= SHIFT_WIDTH;
    }

    /* marking red and white */
    (void) memset(&colors_left[0], 0, sizeof(colors_left));
    red_temp = white_temp = 0;
    for (j = 0; j < SLOTS; ++j) {
        /* mark red */
        if (guess[j] == secret[j]) {
            red_temp++;
        } else {
            colors_left[secret[j]]++;
        }
    }
    for (j = 0; j < SLOTS; ++j) {
        /* not marked red */
        if (guess[j] != secret[j]) {
            if (colors_left[guess[j]] > 0) {
                white_temp++;
                colors_left[guess[j]]--;
            }
        }
    }

    /* build response buffer */
    resp[0] = red_temp;
    resp[0] |= (white_temp << SHIFT_WIDTH);

    return red_temp;
}

static int exclude_combinations(uint8_t redWhiteBuff, uint8_t *prev_guess){
  int red = (redWhiteBuff & 7);
  int white = (redWhiteBuff >> 3) & 7;

  uint8_t buff[1];

  int red_temp;
  int white_temp;
  int count = 0;
  for (size_t i = 0; i < COMBINATIONS; i++) {
    if(allCombinations[i] < COMBINATIONS){
      (void)check_answer(allCombinations[i],buff, prev_guess);
      red_temp = (buff[0] & 7);
      white_temp = (buff[0] >> 3) & 7;
      if(white_temp!=white || red_temp!=red ) {
        allCombinations[i] ^= (1 << 15);
        ++count;
      }
    }
    else{
      ++count;
    }
  }
  return COMBINATIONS - count;
}

static uint16_t compute_patern(void) {
  int bestCount = 0;
  uint16_t selected_colors;
    for (size_t i = 0; i < COMBINATIONS; i++) {
      if(allCombinations[i] < COMBINATIONS){
        uint8_t possibleGuess[SLOTS];
        uint8_t countCoulors[COLORS];
        uint8_t parity_calc = 0;
        uint16_t selected_colors_temp = allCombinations[i];
        for (size_t j = 0; j < COLORS; j++) {
          countCoulors[j] = 16;
        }
        int count = 0;
        for (size_t j = 0; j < SLOTS; ++j) {
            int tmp = selected_colors_temp & 0x7;
            parity_calc ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
            possibleGuess[j] = tmp;
            if(countCoulors[possibleGuess[j]] != possibleGuess[j]){
              countCoulors[possibleGuess[j]] = possibleGuess[j];
              ++count;
              if(count>bestCount){
                bestCount = count;
              }
            }
            selected_colors_temp >>= SHIFT_WIDTH;
        }
        if(count >= bestCount){
          //printf("Count: %d\n", count);
          selected_colors = allCombinations[i];
        }
        if(bestCount>=4){
          break;
        }
      }
    }
    /* calculate parity */
    uint8_t parity_calc = calculate_parity(selected_colors);

    return (selected_colors & 0x7FFF) | (parity_calc << 15);
}

static uint8_t calculate_parity(uint16_t selected_colors){
  int8_t parity_calc = 0;
  for (int i = 0; i < 15; ++i) {
    parity_calc ^= selected_colors;
    selected_colors >>= 0x1;
  }
  return parity_calc &= 0x1;
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

static size_t write_to_server(int fd, uint8_t *buffer, size_t n){

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

static size_t read_from_server(int fd, uint8_t *buffer, size_t n)
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
    DEBUG("Shutting down client\n");
    if (connfd >= 0) {
        (void) close(connfd);
    }
}
