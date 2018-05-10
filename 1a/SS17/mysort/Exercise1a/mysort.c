/**
 * @file mysort.c
 * @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
 * @date 15.03.2017
 *
 * @brief This Program reads input from stdin or a file, 
 *        sorts them via qsort in ascending or descendig order
 *        and prints the result on stdout
 **/
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

typedef struct node { /** < The struct for the LinkedList*/
    char *val;
    struct node * next;
} node_t;

static char *pgm_name; /** < The program name.*/

/**
*@brief This function writes helpful usage information about the program to stderr.
*@details global variables: pgm_name.
*/
static void usage(void);

/**
*@brief pushes new element/line/node to the beginning linked list
*@param pointer to pointer to head of the list
*@param val content of new node
*@details global variables: pgm_name.
*@return Returns number of elements in the linkedlist
*/
static int push(node_t **head, char *val);

/**
*@brief frees the linkedlist the content of its nodes.
*@param head of the linkedlist
*/
static void free_all(node_t *head);

/**
*@brief reads in the arguments
*@param argc argument count
*@oaram argv argument vector
*@return bool reverse.
*/
static bool getarguments(int argc, char *argv[]);

/**
*Program entry point
*@brief allocates the head of the linkedlist,
* reads all files provided as an argument and saves the input to the linkedlist by calling the function "push".
*@param argc The argument counter
*@param argv The argument vector
*@param filesAsArgument number of elements/nodes/lines in the list after the function returns
*@details global variables: pgm_name
*@return Returns the new head of the list
*/
static node_t* readFromFileAndSave(int argc, char *argv[], int *filesAsArgument);

/**
*@brief allocates the head of the linkedlist, reads input from stdin and saves it the linkedlist by calling the function push.
*@param list_length umber of elements/nodes/lines in the list after the function returns
*@details global variables: pgm_name
*@return Returns head of the list
*/
static node_t* readFromStdinAndSave(int *list_length);

/**
*@brief calls the function strcmp to compare to strings
*@return Returns what strcmp returns
*/
static int comp (const void *elem1, const void *elem2);

/**
*@brief calls the function strcmp to compare to strings and reverses the order
*@return Returns what strcmp returns and multiplies by -1
*/
static int compRev (const void *elem1, const void *elem2);

/**
*@brief converts linkedlist into 2d char array.
* sorts the array by calling the function qsort and prints the sorted array.
*@param head is  head of the linkedlist
*@param list_length is the length of the linkedlist
*@param reverse bool to check if qsort has to reverse the order by calling either comp or compRev
*/
static void sortAndPrint(node_t *head, int list_length, bool reverse);

/**
*Program entry point
*@brief saves program name,
* calls the functions getarguments, readFromFileAndSave, sortAndPrint, free_all
* and handles the return values
*@param argc The argument counter
*@param argv The argument vector
*@details global variables: pgm_name
*@return Returns EXIT_SUCCESS
*/
int main(int argc, char *argv[]){
    pgm_name = argv[0];
    bool reverse = getarguments(argc, argv);
    int filesAsArgument = 0;
    node_t *head = readFromFileAndSave(argc, argv, &filesAsArgument);
    if(filesAsArgument == 0){
        head = readFromStdinAndSave(&filesAsArgument);
    }
    sortAndPrint(head, filesAsArgument, reverse);
    free_all(head);
    return EXIT_SUCCESS;
}

static void sortAndPrint(node_t *head, int list_length, bool reverse){
    node_t * current = head;
    char arr[list_length][1024];
    for (int i = 0; current->next != NULL; ++i) {
        bzero(arr[i], 1024);
        strncpy(arr[i], current->val, strlen(current->val));
        current = current->next;
    }
    qsort(&arr[0], list_length, 1024, reverse?compRev:comp);
    for(int i=0; i<list_length; ++i){
        (void)printf("%s", arr[i]);
    }
}

static int comp (const void *elem1, const void *elem2) {
  return (strcmp((char *)elem1,(char *)elem2));
}

static int compRev (const void *elem1, const void *elem2) {
  return (strcmp((char *)elem1,(char *)elem2))*-1;
}

static node_t* readFromStdinAndSave(int *list_length){
    char *buf = malloc(sizeof(char)*1024);
    if(buf==NULL){
        fprintf(stderr, "%s malloc buf failed\n", pgm_name);
        exit(EXIT_FAILURE);
    }
    node_t *head = malloc(sizeof(node_t));
    if(head==NULL){
        fprintf(stderr, "%s malloc head failed\n", pgm_name);
        free(buf);
        exit(EXIT_FAILURE);
    }
    head->next = NULL;
    while(fgets(buf, 1022, stdin)!=0){
        *list_length = push(&head, buf);        
    }
    free(buf);
    return head;
}

static node_t* readFromFileAndSave(int argc, char *argv[], int *filesAsArgument){
    char *buf = malloc(sizeof(char)*1024);
    if(buf==NULL){
        fprintf(stderr, "%s malloc buf failed\n", pgm_name);
        exit(EXIT_FAILURE);
    }
    node_t *head = malloc(sizeof(node_t));
    if(head==NULL){
        fprintf(stderr, "%s malloc head failed\n", pgm_name);
        free(buf);
        exit(EXIT_FAILURE);
    }
    head->next = NULL;
    for(int i=1; i<argc; ++i){
        FILE *file = fopen(argv[i], "r");
        if(file!=NULL){
            while(fgets(buf, 1022, file)!=NULL){
                *filesAsArgument = push(&head, buf);  
            }
            fclose(file);
        }
    }
    free(buf);
    return head;
}

static bool getarguments(int argc, char *argv[]){
    int option;
    bool reverse = false;
    while((option = getopt(argc, argv, "r"))!=-1){
        switch(option){
            case 'r': 
                reverse = true;
                break;
            case '?':
                usage();
                exit(EXIT_FAILURE);
            default:
                assert(0); //unreachable
        }
    }
    return reverse;
}

static void usage(void){
    (void)fprintf(stderr, "%s [options] [file1]...\n",pgm_name);
    (void)fprintf(stderr, "Ordering options:\n");
    (void)fprintf(stderr, "-r     reverse the result of comparisons\n");
}

static int push(node_t **head, char *val){
    static int list_length = 0;
    ++list_length;
    node_t * new_node;
    new_node = malloc(sizeof(node_t) + sizeof(char*)*1024);
    if(new_node==NULL){
        (void)fprintf(stderr, "%s: malloc push failed\n", pgm_name);
        free_all(*head);
        exit(EXIT_FAILURE);
    }
    new_node->val = malloc(sizeof(char*)*1024);
    if(new_node->val==NULL){
        (void)fprintf(stderr, "%s: malloc push new val failed\n", pgm_name);
        free_all(*head);
        exit(EXIT_FAILURE);
    }
    strcpy(new_node->val, val);
    new_node->next = *head;
    *head = new_node;
    return list_length;
}

static void free_all(node_t *head){
    node_t * current = head;
    node_t * prev;
    while (current->next != NULL) {
        prev = current;
        current = current->next;
        free(prev->val);
        free(prev);
    }
    free(prev->next);
}