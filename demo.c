// demo.c
// CS 58
// sample code to collect input from the user

// sometimes, when you want both single-char input and longer text input,
// using scanf and getc and such, one fetch may leave unconsumed characters
// in stdin that confuses subsequent fetches.  
// trailing newlines also can cause trouble.
//
// should these things cause you problems in proj1, this code may
// prove helpful

// adam salem, s.w. smith
// updated sep 2020

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRING_LEN  32

// prompt the user with message, and save input at buffer
// (which should have space for at least len bytes)
int input_string(char *message, char *buffer, int len) {

  int rc = 0, fetched, lastchar;

  if (NULL == buffer)
    return -1;

  if (message)
    printf("%s: ", message);

  // get the string.  fgets takes in at most 1 character less than
  // the second parameter, in order to leave room for the terminating null.  
  // See the man page for fgets.
  fgets(buffer, len, stdin);

  fetched = strlen(buffer);


  // warn the user if we may have left extra chars
  if ( (fetched + 1) >= len) {
    fprintf(stderr, "warning: might have left extra chars on input\n");
    rc = -1;
  }

  // consume a trailing newline
  if (fetched) {
    lastchar = fetched - 1;
    if ('\n' == buffer[lastchar])
      buffer[lastchar] = '\0';
  }

  return rc;
}


// testing out the above code
/*int main(int argc, char * argv[]) {

  char *buffer;

  buffer = (char *)malloc(STRING_LEN);

  if (NULL == buffer) {
    fprintf(stderr,"malloc error\n");
    return -1;
  }
   

  input_string("enter y/n", buffer,STRING_LEN);
  printf("that was [%s]\n", buffer);

  input_string("enter name", buffer,STRING_LEN);
  printf("that was [%s]\n", buffer);

  free(buffer);

  return 0;
}
*/
  
