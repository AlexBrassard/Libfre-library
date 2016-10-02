#include <stdio.h>
#include <stdlib.h>

#include <fre.h>

int main(int argc, char **argv)
{
  int retval = 0;

  if (argc < 3){
    fprintf(stderr, "\nUsage:  %s [pattern] [string]\n\n", argv[0]);
    return -1;
  }

  retval = fre_bind(argv[1], argv[2]);
  return retval;
}
