#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <fre.h>
#include "fre_internal.h" /* print_ptable_hook() only */

#define DEF_ARR_SIZE 1024

void usage(char *name)
{
  fprintf(stderr, "\nUsage:  %s [pattern] [string]\n\n", name);
}

int main(int argc, char **argv)
{
  int retval = 0;
  size_t i = 0;
  size_t string_len = 0;
  size_t new_lenght = DEF_ARR_SIZE;
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
	if ((new_lenght *= 2) >= SIZE_MAX-1){
	  fprintf(stderr, "%s: Input too large\n\n", __func__);
	  errno = EOVERFLOW;
	  return -1;
	}
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
  printf("Input: %s\n\n", string);
  if ((retval = fre_bind(argv[1], string, new_lenght)) == -1)
    FRE_PERROR("fre_bind");
  print_ptable_hook();
  printf("Output: %s\n\n", string);
  if (string){
    free(string);
    string = NULL;
  }

  return retval;
}
