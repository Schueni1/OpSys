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

uint16_t allCombinations[COMBINATIONS];

/* Name of the program */
static const char *progname = "client";

/* File descriptor for connection socket */
static int connfd = -1;

static int red, white;


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
 * Tries different color combinations
 *
 * @brief
 * @param try  an int specifying the number of the current try
 * @return     two bytes containing the color information and a parity bit
 */
uint16_t compute_patern(int try);

uint16_t compute_patern_intelligent(void);

void compute_all_combinations(void);

int exclude_combinations(uint8_t *buff, uint8_t *prev_guess);

int check_answer(uint16_t req, uint8_t *resp, uint8_t *secret);

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

static uint8_t calculate_parity(uint16_t selected_colors);

/**
 * @brief free allocated resources
 */
static void free_resources(void);

static size_t write_to_server(int fd, uint8_t *buffer, size_t n);
static size_t read_from_server(int fd, uint8_t *buffer, size_t n);
