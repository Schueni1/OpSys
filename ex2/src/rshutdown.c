/**
 * @file rshutdown.c
 * @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
 * @date 16.04.2017
 *
 * @brief prints either "KaBOOM" on stdout and exits witch exitcode "EXIT_FAILURE" with a 2/3 Chance 
or prints "SHUTDOWN COMPLETED" on stdout and returns "EXIT_SUCCESS".
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
*Program entry point
*@brief prints either "KaBOOM" on stdout and exits witch exitcode "EXIT_FAILURE" with a 2/3 Chance 
or prints "SHUTDOWN COMPLETED" on stdout and returns "EXIT_SUCCESS".
*@param argc The argument counter
*@param argv The argument vector
*@return Returns EXIT_SUCCESS with 1/3 chance.
*/
int main(int argc, char **argv){
    if(argc!=1){
        (void)fprintf(stderr, "usage:\n%s\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    time_t seed = time(NULL);
    long int temp = (long int) &seed;
    int i = (int) temp % 10000000;
    if(seed == (time_t) -1){
        fprintf(stderr, "%s time call failed\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    srand(seed - i); /**< starts srand with current time minus address of variable seed to ensure diffrent values each execution
                        of the program. Substracting the address ensures a diffrent value even if the program is executed
                        multiple times within a second */
    if(rand()%3!=0){
        (void)printf("KaBOOM!\n");
        exit(EXIT_FAILURE);
    }
    (void)printf("SHUTDOWN COMPLETED\n");
    return EXIT_SUCCESS;
}
