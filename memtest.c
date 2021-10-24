#include <stdlib.h>
#include <stdio.h>

int main (int argc, char **argv){

  char* x = gc_malloc(24);
  char* y = gc_malloc(19);
  char* z = gc_malloc(32);
  
  printf("x = %p\n", x);
  printf("y = %p\n", y);
  printf("z = %p\n", z);

}
