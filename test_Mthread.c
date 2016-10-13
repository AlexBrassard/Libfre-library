#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fre.h>


#define DEF_ARR_SIZE 1024

struct test_arg {
  char *string;
  char *pattern;
  size_t string_lenght;
};

void* fre_start_thread(void *arg)
{
  struct test_arg *original = (struct test_arg *)arg;
  fre_bind(original->pattern, original->string, original->string_lenght);
  printf("Output:\n\n%s\n", original->string);
  original = NULL;
  pthread_exit((void*)1);
}

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
  size_t tnum = 8;
  pthread_t threads[8];
  struct test_arg arg;

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

  arg.string = string;
  arg.pattern = argv[1];
  arg.string_lenght = new_lenght;
  for (i = 0; i < tnum; i++)
    if (pthread_create(&threads[i], NULL, fre_start_thread, (void *)&arg) != 0){
      perror("Pthread_create");
      return -1;
    }
  for (i = 0; i < tnum; i++)
    pthread_join(threads[i], NULL);


  if (string){
    free(string);
    string = NULL;
  }

  return retval;
}
