

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
// #include <xlocale.h>

/**
* @brief checks if a word is a palindrom
* @param palindrom is word to check if it is a palindrom
* @param ignoreCase indicates if it should ignore the case
  (true = ignore the case, false = don't ignore the case)
* @return true if it is a palindrom, false if it is not a palindrom
*/
bool ispalindrom(char palindrom[], bool ignoreCase);

int main(int argc, char *argv[]){
  char palindrom[42];
  bool ignoreCase = false;
  bool ignoreTab = false;
  int option;
  while ((option = getopt(argc, argv, "si")) != -1) {
    switch (option) {
      case 'i':
      if(ignoreCase==true){
        fprintf(stderr, "%s\n", "Flag \"i\" had been set more than one time");
        exit(1);
      }
      ignoreCase = true;
      break;

      case 's':
      if(ignoreTab==true){
        fprintf(stderr, "%s\n", "Flag \"s\" had been set more than one time");
        exit(1);
      }
      ignoreTab = true;
      break;

      case '?':
      fprintf(stderr, "%s\n", "Set flag -i to set case insensitiv and flag -s to ignore tabs");
      exit(1);
      break;

      default:
      assert(0); //unreachable
      break;
    }
  }

  while(1){
    //waits for input and writes it into palindrom array
    //quits if EOF (end of File)
    if (fgets(palindrom, 42, stdin) == NULL) {
      return 0;
    }
    //checks if is not longer than 40 characters
    if (strchr(palindrom,'\n')) {
      size_t ln = strlen(palindrom)-1;
      //delete newline
      if (palindrom[ln] == '\n'){
        palindrom[ln] = '\0';
      }

      //cuts the string at ' ' (tab) if flag -s (ignoreTab) had been set
      if((ignoreTab==true && (strchr(palindrom,' ')!=NULL))){
        printf("%s", palindrom);
        for(int i=0 ; i<42; i++){
          if(palindrom[i]==' '){
            palindrom[i] = '\0';
          }
        }
        (void)printf(" ist %s Palindrom\n", ispalindrom(palindrom, ignoreCase)?"ein":"kein");
      }
      else{
        (void)printf("%s ist %s Palindrom \n",palindrom, ispalindrom(palindrom, ignoreCase)?"ein":"kein");
      }
    }
    else{
      (void)fprintf(stderr,"ispalindrom: Eingabe zu lang, max 40 Zeichen!\n");
      if(strchr(palindrom, '\n')==NULL)     //newline does not exist
      while(fgetc(stdin)!='\n');          //discard until newline
    }
  }
  return 0;
}

bool ispalindrom(char palindrom[], bool ignoreCase){
  //get length of the word/line
  int length;
  for(int i = 0; i<41; i++){
    if(palindrom[i] == 0){
      length = i;
      break;
    }
  }

  char* partOne =(char*) malloc((length>1?length/2:1)+2);
  char* partTwo =(char*) malloc((length>1?length/2:1)+2);
  partOne[length/2] = '\0';
  partOne[length/2+1] = '\n';
  partTwo[length/2] = '\0';
  partTwo[length/2+1] = '\n';
  for (int i = 0; i < length/2; i++) {
    partOne[i] = palindrom[i];
    partTwo[i] = palindrom[length-i-1]; //changes the order of the second half
  }
  if(ignoreCase==true){
   if(strncasecmp(partOne, partTwo, length/2)!=0){
    return false;
  }
  }
  else if(strcmp(partOne, partTwo)!=0){
    return false;
  }
  free(partOne);
  free(partTwo);
  return true;
}
