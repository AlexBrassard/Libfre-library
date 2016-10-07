#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fre.h>


#define DEF_ARR_SIZE 1024

void usage(char *name)
{
  fprintf(stderr, "\nUsage:  %s [pattern] [string]\n\n", name);
}

int main(int argc, char **argv)
{
  size_t i = 0;
  int retval = 0;
  int string_len = 0;
  int new_lenght = DEF_ARR_SIZE;
  char input;
  char *string = NULL;
  char *temp = NULL;

  if ((string = calloc(DEF_ARR_SIZE, sizeof(char))) == NULL){
    perror("calloc");
    return -1;
  }
  
  if (argc < 2) {
    usage(argv[0]);
    return -1;
  }
  else if (argc < 3){
    while ((input = getchar()) != EOF) {
      if (++string_len >= new_lenght){
	new_lenght *= 2;
	if ((temp = realloc(string, new_lenght * sizeof(char))) == NULL){
	  perror("Realloc");
	  return -1;
	}
	string = temp;
      }
      string[i++] = input;
    }
    string[i] = '\0';
  }
  else {
    memcpy(string, argv[2], DEF_ARR_SIZE);
    string[DEF_ARR_SIZE - 1] = '\0';
  }

  retval = fre_bind(argv[1], string);
  if (string){
    free(string);
    string = NULL;
  }

  return retval;
}
