#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <strings.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#define MSGSIZE 1024

struct arr{
  char a[MSGSIZE][MSGSIZE];
};
struct arrMerged{
  char a[MSGSIZE*2][MSGSIZE];
};

int merge(struct arr *a, int m, struct arr *b, int n, struct arrMerged *sorted);

int comp (const void *elem1, const void *elem2);

void exAndSave(char *command, struct arr *inbuf);

void writeToStdout(char *command, struct arrMerged *content, int size);

int mergeAndSortArrays(struct arr *inbuf1, struct arr *inbuf2, struct arrMerged *inbufMerged);


int main(int argc, char *argv[]){
   if(argc!=3){
	    (void)fprintf(stderr,"%s", "WRONG ARGUMENT COUNT\n");
      (void)printf("USAGE:\n");
      (void)printf("%s \"<command1>\" \"<command2>\"\n", argv[0]);
	    exit(1);
   }

   char *command1 = argv[1];
   char *command2 = argv[2];
   struct arr inbuf1;
   struct arr inbuf2;
   struct arrMerged inbufMerged;
   for (size_t i = 0; i < MSGSIZE*2; i++) {
     bzero(inbufMerged.a[i], sizeof(inbufMerged.a[i]));
   }
   for (size_t i = 0; i < MSGSIZE; i++) {
     bzero(inbuf1.a[i], sizeof(inbuf1.a[i]));
     bzero(inbuf2.a[i], sizeof(inbuf2.a[i]));
   }

   exAndSave(command1, &inbuf1);
   exAndSave(command2, &inbuf2);

   int size = mergeAndSortArrays(&inbuf1, &inbuf2, &inbufMerged);

   writeToStdout("uniq -d", &inbufMerged, size);

   return 0;
}

int comp (const void *elem1, const void *elem2) {
  return (strcmp((char *)elem1,(char *)elem2));
}

void exAndSave(char *command, struct arr *inbuf){
  int p[2], pid;

  for (size_t i = 0; i < MSGSIZE; i++) {
    bzero(inbuf->a[i], sizeof(inbuf->a[i]));
  }

  /* open pipe */
  if(pipe(p) == -1){
    (void)perror("pipe call error");
    exit(1);
  }

  switch(pid = fork()){
    case -1:
    (void)perror("error: fork call");
    exit(2);

    case 0:  /* if child then write down pipe */
      if(close(p[0])<0){  /* first close the read end of the pipe */
        (void)perror( "dup2 failed" );
        exit(1);
      }
      if(dup2(p[1], STDOUT_FILENO) == -1){ /* stdout == write end of the pipe */
         (void)perror( "dup2 failed" );
         exit(1);
      }
      if(close(p[1])<0){
        (void)perror("error: closing write end");
        exit(1);
      }
      char* argz[] ={"/bin/bash", "-c",command, NULL};
      execv(argz[0], argz);
      (void)fprintf(stderr, "%s\n", "execv failed");
      exit(1);
    default:   /* parent reads pipe */
        if(close(p[1])<0){ /* first close the write end of the pipe */
          (void)perror("error: closing write end");
          exit(1);
        }
        if(dup2(p[0], STDIN_FILENO ) == -1 ){ /* stdin == read end of the pipe */
           (void)perror( "dup2 failed" );
           _exit(1);
        }
        if(close(p[0])<0){ /*close the fd of read end of the pipe */
          (void)perror("error: closing read end");
          exit(1);
        }
        char tempBuff[1024];
        for (int i=0; fgets(tempBuff, 1024, stdin)!=NULL; i++) {
          (void)strncpy(inbuf->a[i], tempBuff, strlen(tempBuff));
        }
        if(wait(NULL)<0){
          (void)perror("error: wait");
          exit(1);
        }
   }
}

void writeToStdout(char *command, struct arrMerged *content, int size){
  int p[2], pid;

  /* open pipe */
  if(pipe(p)==-1){ /* Parent read/child write pipe */
    (void)perror("pipe call error");
    exit(1);
  }

   switch(pid=fork()){
     case -1:
        perror("error: fork call");
        exit(1);
     case 0:  /* if child then write down pipe */
       if(close(p[1])){
         (void)perror("error: closing write end");
         exit(1);
       }
       (void)close(STDIN_FILENO);
       if(dup2(p[0], STDIN_FILENO) == -1){
         (void)perror("error: fork call");
         exit(1);
       }
       (void)close(p[0]);
       char* argz[] ={"/bin/bash", "-c",command, NULL};
       execv(argz[0], argz);
       (void)fprintf(stderr, "%s\n", "execv failed");
       exit(1);
     default:   /* parent reads pipe */
      if(close(p[0])){ /* first close the read end of the pipe */
        (void)perror("error: closing read end");
        exit(1);
      }
      for (size_t i = 0; i < size; ++i) {
       int ret = write(p[1], content->a[i], strlen(content->a[i]));
       if(ret==-1){
         (void)fprintf(stderr, "%s\n", "write error");
         exit(1);
       }
      }
      if(close(p[1])<0){
        (void)perror("error: fail closing write end");
        exit(1);
      }
      if(wait(NULL)<0){
        (void)perror("error: wait");
        exit(1);
      }
     }
}

int mergeAndSortArrays(struct arr *inbuf1, struct arr *inbuf2, struct arrMerged *inbufMerged){

  int size1 = 0;
  for (size_t i = 0; i < MSGSIZE; i++) {
    if(inbuf1->a[i][0]=='\0'){
      size1 = i;
      break;
    }
  }
  qsort(inbuf1->a, size1, sizeof(*inbuf1->a), comp);
  int size2 = 0;
  for (size_t i = 0; i < MSGSIZE; i++) {
    if(inbuf2->a[i][0]=='\0'){
      size2 = i;
      break;
    }
  }
  qsort(inbuf2->a, size2, sizeof(*inbuf2->a), comp);

  return merge(inbuf1, size1, inbuf2, size2, inbufMerged);

}

int merge(struct arr *a, int m, struct arr *b, int n, struct arrMerged *sorted) {
  int i, j, k;

  j = k = 0;

  for (i = 0; i < m + n;) {
    if (j < m && k < n) {
      if (strcmp((char *)a->a[j],(char *)b->a[k])<0) {
        (void)strncpy(sorted->a[i], a->a[j], strlen(a->a[j]));
        j++;
      }
      else {
        (void)strncpy(sorted->a[i], b->a[k], strlen(b->a[k]));
        k++;
      }
      i++;
    }
    else if (j == m) {
      for (; i < m + n;) {
        (void)strncpy(sorted->a[i], b->a[k], strlen(b->a[k]));
        k++;
        i++;
      }
    }
    else {
      while (i < m + n) {
        (void)strncpy(sorted->a[i], a->a[j], strlen(a->a[j]));
        j++;
        i++;
      }
    }
  }
  return n+m;
}
