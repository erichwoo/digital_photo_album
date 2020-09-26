/* Author: Erich Woo
 * Date: 20 September 2020
 * A Digital Photo Album that takes in a set of photos, converts, orients, and captions them
 * to the user's liking, and writes them back to the current directory
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "demo.h"

#define STRING_LEN 50
#define RPIPE 0  // the pipe we read from
#define WPIPE 1  // the pipe we write to

/* Determines whether the first 8 bytes in arg param,
 * which encapsulate at least the file header bytes,
 * match an image file of type: jpg, png, bmp, or gif
 * https://web.archive.org/web/20090302032444/http://www.mikekunz.com/image_file_header.html
 * 
 * @param bytes a char array of 8 bytes to match 
 * @return 1 if bytes represent an image, 0 if not 
 */
static int header_is_img(unsigned char bytes[8]) {
  unsigned char jpg[2] = {0xff, 0xd8};
  unsigned char png[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  unsigned char bmp[2] = {0x42, 0x4d};
  unsigned char gif[3] = {0x47, 0x49, 0x46};
  
  return (
    (bytes[0] == jpg[0] && bytes[1] == jpg[1]) ||
    (bytes[0] == png[0] && bytes[1] == png[1] && bytes[2] == png[2] && bytes[3] == png[3] && bytes[4] == png[4] && bytes[5] == png[5] && bytes[6] == png[6] && bytes[7] == png[7]) ||
    (bytes[0] == bmp[0] && bytes[1] == bmp[1]) ||
    (bytes[0] == gif[0] && bytes[1] == gif[1] && bytes[2] == gif[2]));
}

/* Checks if the file given is a valid path & a valid image file
 *
 * @param path the file path 
 * @return -1 if invalid, 0 if valid
 */
static int invalid_img(char* path) {
  FILE* fp;
  unsigned char bytes[8];

  // check readable path
  if ((fp = fopen(path, "r")) == NULL) 
    return -1;

  // check valid image file
  fread(bytes, 8, 1, fp);    // read first 8 bytes of file, which identify if it is img
  if (!header_is_img(bytes)) // if bytes dont match img headers, -1
    return -1;
   
  fclose(fp);
  return 0;
}

/* Validates command-line args from main().
 * Checks if there is at least 1 img argument,
 * and if the files are valid image paths
 * 
 * @param argc the arg count
 * @param argv the args
 * @return -1 if invalid params, 0 if valid
 */
static int validate(int argc, char* argv[]) {
  int i; 

  // not enough args(2)
  if (argc <= 1) {  
    fprintf(stderr, "Usage: ./album [img]+\n");
    return -1;
  }

  // not valid path and image
  for (i = 1; i < argc; i++) {
    if (invalid_img(argv[i])) {
      fprintf(stderr, "Error: one (or more) img is not a valid image or path: %s\n", argv[i]);
      return -1;
    }
  }

  return 0;
}

/* Forks a new process, resizes an image and renames it. 
 * - The child process will call exec() to launch a new program, magick resize,
 * and should exit the program via magick.
 * - The parent process returns the child's pid after fork
 *
 * size is in units of %, and the char* must contain a number folloed by "%"
 *
 * Assumptions: img is a valid image file path, size is valid input 
 * for magick command. Since function is internal, I can ensure 
 * valid resize() function calls.
 *
 * @param img the image to resize
 * @param rename the filename to rename to after resizing
 * @param size the size to resize to
 * @return the pid of the child process
 */
static int resize(char* img, char* rename, char* size) {
  int pid;
  if ((pid = fork()) == 0) {  
#ifdef VERBOSE
    printf("resizing %s now by %s...\n", img, size);
#endif
    execlp("magick", "magick", "convert", "-resize", size, img, rename, NULL);

    // if exec errors
    fprintf(stderr, "Failed to exec() on magick's resize command\n");
    exit(-1);
  }

  return pid;
}

/* Forks a new process and displays an image. 
 * - The child process will call exec()to launch a new program, magick display,
 * and should exit the program via magick.
 * - The parent process returns the child's pid after fork
 *
 * Assumptions: same as resize() assumption; see above.
 *
 * @param img the image to display
 * @return the pid of the child process
 */
static int display(char* img) {
  int pid;
  if ((pid = fork()) == 0) {  
#ifdef VERBOSE
    printf("displaying %s now...\n", img);
#endif
    execlp("magick", "magick", "display", img, NULL);
    
    // if exec errors
    fprintf(stderr, "Failed to exec() on magick's display command\n");
    exit(-1);
  }

  return pid;
}

/* Forks a new process, rotates the image and renames it. 
 * - The child process will call exec() to launch a new program, magick rotate,
 * and should exit the program via magick.
 * - The parent process returns the child's pid after fork
 *
 * If rot_dir = 1, 90 degrees clockwise
 *    rot_dir = 2, 90 degrees counter-clockwise
 *    otherwise, no rotation
 *
 * Assumptions: same as resize() assumption; see above.
 *
 * @param img the image to rotate
 * @param rename the new name to rename to
 * @param rot_dir the direction to rotate
 * @return the pid of the child process
 */
static int rotate(char* img, char* rename, int rot_dir) {
  int pid;
  if ((pid = fork()) == 0) {  
#ifdef VERBOSE
    printf("rotating %s now in dir: %d\n", img, rot_dir);
#endif
  
    char* direction;
  
    if (rot_dir == 1)
      direction = "90";
    else if (rot_dir == 2)
      direction = "-90";
    else
      direction = "0";  // failsafe if rot_dir is weird input
    
    execlp("magick", "magick", "convert", "-rotate", direction, img, rename, NULL);
  
    // if exec errors
    fprintf(stderr, "Failed to exec() on magick's rotate command.\n");
    exit(-1);
  }

  return pid;
}

/* Creates a pipe between parent and child to communicate
 * about asking the user if they want to rotate the image
 * and desired caption. Utilizes fork() to create separate 
 * child process. The parent tells the child when it can begin
 * prompting the user for rotation, then reads what the child gathered 
 * from user input. The child waits for the parent to tell when it 
 * can ask the user about rotation & caption. ask_user() will not return
 * if in child process, instead will exit(0).
 *
 * Note: parent process continues after exiting function ask_user, but
 * child continues in ask_user() if-block after parent exits function.
 * Parent will later retrieve caption user input outside of ask_user()
 *
 * @param sv1 the pipe from parent to child
 * @param sv2 the pipe from child to parent
 * @return -1 on error creating pipe or child,
 *          0 if no rotation requested, 
 *          1 if clockwise,
 *          2 if counter-clockwise
 */
static int ask_user(int sv1[2], int sv2[2]) {
  int child_pid = 0;

  // create pipe and validate its creation
  if (pipe(sv1) != 0 || pipe(sv2) != 0) {
    fprintf(stderr, "failed to create a pipe\n");
    return -1;
  }

  // fork and validate
  if ((child_pid = fork()) < 0) {
    fprintf(stderr, "failed to fork child\n");
    return -1;
  }
  if (child_pid == 0) {
    /******************** child process *******************/
    int from_parent, to_parent;
    int i, receive;
    char* message;
    char* writebuf = (char*) malloc(STRING_LEN * sizeof(char));

    close(sv1[WPIPE]);
    close(sv2[RPIPE]);

    from_parent = sv1[RPIPE];
    to_parent = sv2[WPIPE];
    
    // iterate once each for rotation & caption user input
    // each time, wait for parent to start prompt, then ask
    // and write back user input to parent
    for (i = 0; i < 2; i++) {     
      if (i == 0)
	message = "Rotate the photo clockwise(1), counter-clockwise(2), or not rotate at all(3)?\n";
      else if (i == 1)
	message = "What's the  caption for this photo?\n";

      // wait for parent to tell me when to ask user
      // not retrieving any data, just need read() to induce waiting
      // receive is dummy variable so read() works correctly
      if (read(from_parent, &receive, sizeof(int)) < 0)
	fprintf(stderr, "error reading bytes from parent\n");

      // ask user for rotate/caption           
      input_string(message, writebuf, STRING_LEN);
      
      // write user's answer to parent
      if (write(to_parent, writebuf, STRING_LEN) < 0)
	fprintf(stderr, "error writing bytes to parent\n");
    }

    free(writebuf);
    // exit from child
    exit(0);
  }
  int send = 0; // just like 'receive' above, send is dummy variable
  int rot_dir = 0; // rot_dir defaults to no rotation aka 0
  int from_child, to_child;
  char readbuf[STRING_LEN];

  close(sv1[RPIPE]);
  close(sv2[WPIPE]);

  from_child = sv2[RPIPE];
  to_child = sv1[WPIPE];
  
  // tell child to start asking user for rotate
  if (write(to_child, &send, sizeof(int)) < 0)
    fprintf(stderr, "error writing bytes to child\n"); // keep going regardless

#ifdef VERBOSE
  printf("----waiting for user input on rotation\n");
#endif

  // read what user inputted for rotation
  if (read(from_child, readbuf, STRING_LEN) < 0)
    fprintf(stderr, "error reading bytes from child\n"); // keep going regardless

#ifdef VERBOSE
  printf("--done waiting for user input, captured '%s'\n", readbuf);
#endif

  // if inputted 1 aka clockwise
  if (strcmp(readbuf, "1") == 0)
    rot_dir = 1;
  // if inputted 2 aka counter-clockwise
  else if (strcmp(readbuf, "2") == 0) 
    rot_dir = 2;
  
  return rot_dir;
}

/* Completes the waiting child from ask_user()
 * and retrieves the caption from user input. 
 * reuses the created pipes from ask_user().
 * 
 * @param sv1 the pipe from parent to child
 * @param sv2 the pipe from child to parent
 * @param caption the caption to retrieve
 * @return 0 on success, -1 on pipe-reading error
 */
static int ask_caption(int sv1[2], int sv2[2], char* caption) {  
  int from_child = sv2[RPIPE];
  int to_child = sv1[WPIPE];
  int send = 0; // send signal value is irrelavent
  
  // tell child to start asking user for caption
  if (write(to_child, &send, sizeof(int)) < 0) 
    fprintf(stderr, "error writing bytes to child\n"); // keep reading regardless

#ifdef VERBOSE
  printf("----waiting for user input on caption\n");
#endif
  
  // read what user decieded from child
  if (read(from_child, caption, STRING_LEN) < 0) {
    fprintf(stderr, "error reading bytes from child\n");
    return -1;
  }

#ifdef VERBOSE
  printf("--done waiting for user input, captured %s\n", caption);
#endif
  
  return 0;
}

/* Creates or edits an "index.html" file, writing
 * the thumbnail linked by a medium-sized image.
 * cap_html() will later finish the writing for individual 
 * image by writing the caption.
 *
 * Note: index = 0 signifies the first image to add to html.
 * In that case, if "index.html" already exists, 
 * overwrite it with "w" - write, instead of "a" - append
 *
 * @param thumb_name the thumbnail
 * @param med_name, the medium-sized image
 * @param index the index of photo to write to html
 * @return -1 on error, 0 on successful write
 */

static int img_html(char* thumb_name, char* med_name, int order) {
#ifdef VERBOSE
  printf("writing img to html now for %s...\n", thumb_name);
#endif
  
  FILE* fp;
  char* w_or_a = "a";

  // overwrite "index.html" if first photo to edit
  if (order == 1)
    w_or_a = "w";
  
  if ((fp = fopen("index.html", w_or_a)) == NULL) {
    fprintf(stderr, "Error writing to index.html\n");
    return -1;
  }
  fprintf(fp, "<a href=\"%s\"><img src=\"%s\"></a>", med_name, thumb_name);
  
  fclose(fp);
  return 0;
}

/* Write the caption of image to "index.html"
 *
 * @param caption the caption
 * @return -1 on error, 0 on successful write
 */
static int cap_html(char* caption) {
#ifdef VERBOSE
  printf("adding caption %s to html now...\n", caption);
#endif
  
  FILE* fp;
  
  if ((fp = fopen("index.html", "a")) == NULL) {
    fprintf(stderr, "Error writing to index.html\n");
    return -1;
  }
  fprintf(fp, "<h2>%s</h2>", caption);
  
  fclose(fp);
  return 0;
}

/* Executes the image editing process for one image, including:
 *   1. resizing 10% for thumbnail and adding to directory
 *   2. displaying thumbnail
 *   3. asking the user whether to rotate (and rotating if so)
 *   4. asking the user for a caption
 *   5. resizing 25% and rotating (if desired) for medium-sized image and adding to directory
 *   6. adding the thumbnail, caption, and link from thumbnail -> medium to "index.html"
 *
 * Assumptions: assumes params have been validated for correctness
 *
 * @param img the image to process
 * @param thumb_name the desired thumbnail name
 * @param med_name the desired medium-sized image name
 * @param index the index of the image
 * @param ptp the pipe for communicating about displaying the next image
 * @param html_order the pipe for communicating about which write_html process to write next to html
 * @return 0 on successful processing, -1 otherwise
 */
static int process_img(char* img, char* thumb_name, char* med_name, int index, int ptp1[2], int ptp2[2]) {
  int res_thumb, dis_thumb, rot_dir, status;
  int sv1[2], sv2[2]; // sv1 = parent -> child; sv2 = child -> parent
  int send = 0, receive;
  int to_next = ptp1[WPIPE];    // ptp1 is from img_process to next img_process
  int from_prev = ptp1[RPIPE];
  int html_out = ptp2[WPIPE];  // ptp2 is from any img_process to the
  int html_in = ptp2[RPIPE];   // next img_process that comes through the processor
  char caption[STRING_LEN];
  
  index++; // change index to cardinal starting at 1 instead of 0 for readability

//////////////////////////// RESIZING //////////////////////////////////

  /*********** Fork for resizing thumbnail ***********/
#ifdef VERBOSE
  printf("------%d forking for thumb resize\n", index);
#endif
  res_thumb = resize(img, thumb_name, "10%");
  
  /*********** Fork for medium resize **********/        
#ifdef VERBOSE                                           
  printf("------%d forking for med resize\n", index);              
#endif
  resize(img, med_name, "25%");              

//////////////////////// WAIT FOR PREV IMG TO FINISH TO CONTINUE /////////////////////

// can't display next and write html for next until caption
// is asked and caption is written to html, respectively
// SPECIAL CASE: First image  
  if (index != 1) {
    // want to wait until previous html image is set
    // aka after caption from prev image is written to html
    int cont = 1;                           
    while (cont) {
      // read signal for whose turn it is to write html
      if (read(html_in, &receive, sizeof(int)) < 0) {
	fprintf(stderr, "error reading bytes from other img process. exiting...\n");
	exit(-1);
      }
#if defined (VERBOSE) || (WAIT)
      printf("%d received %d\n", index, receive);
#endif
      // send the same write signal right back out if
      // receive signal != your index
      // aka NOT your turn to write html
      if (index != receive) {
	if (write(html_out, &receive, sizeof(int)) < 0) {
	  fprintf(stderr, "error writing bytes out other img process. exiting...\n");
	  exit(-1);
	}
#if defined (VERBOSE) || (WAIT)
	printf("%d resending %d\n", index, receive);
#endif
	sleep(1); // move out of the way
      }
      else  // continue out of while loop if it is your turn to write
	cont = 0;
    }
  }
  
//////////////// START WRITING HTML AND DISPLAYING /////////////////
  
/*********** Fork for adding photos to html *********/

#ifdef VERBOSE
  printf("------%d forking for img add to html\n", index);
#endif 
  if (fork() == 0)
    exit(img_html(thumb_name, med_name, index));

//////////////////////////// DISPLAYING ///////////////////////////////

  // Make sure thumbnail is resized already
#ifdef VERBOSE
  printf("---%d waiting for thumb resize\n", index);
#endif
  waitpid(res_thumb, &status, 0);
#ifdef VERBOSE
  printf("---%d done waiting for thumb resize\n", index);
#endif
  
  // wait for previous img conversion process to finish
  // asking user before displaying next image,
  // special case don't wait if it is the 1st conversion
  if (index != 1) {
#if defined (VERBOSE) || (WAIT)
    printf("---%d waiting to display till  user finished previous img\n", index);
#endif
    if (read(from_prev, &receive, sizeof(int)) < 0) 
      fprintf(stderr, "error reading bytes from other img process\n"); // keep going anyway, read data irrelavent
#if defined (VERBOSE) || (WAIT)
    printf("---%d done waiting on previous img\n", index);
#endif
  }
    
  /************ Fork for displaying thumbnail **********/
#ifdef VERBOSE
  printf("------%d forking for thumb display\n", index);
#endif
  printf("=============== %s ===============\n", img);
  printf("Please close the image to continue!\n");
  dis_thumb = display(thumb_name);

///////////////////////////// ASKING ////////////////////////////

// Make sure display has been closed by user before asking
#ifdef VERBOSE 
  printf("---%d waiting for thumb display\n", index);
#endif
  waitpid(dis_thumb, &status, 0);
#ifdef VERBOSE
  printf("---%d done waiting for thumb display\n", index);
#endif

  /*********** asking to rotate ************/

  // forking occurs here
  // child survives past function return in parent process
  // child asks for user input twice (rotate & caption)
  // parent in function only fetches rotate user input
  rot_dir = ask_user(sv1, sv2);
  
  /************ Fork for rotating thumb and med **********/

  // non-zero rot_dir means rotation is desired
  if (rot_dir > 0) {
#ifdef VERBOSE
    printf("------%d forking for thumb rotate\n", index);
#endif
    rotate(thumb_name, thumb_name, rot_dir);
      
#ifdef VERBOSE
    printf("------%d forking for med rotate\n", index);
#endif
    rotate(med_name, med_name, rot_dir);
  }
  
  /******** asking for caption ********/

  // calls child from ask_user to 
  // finish asking its second question
  // ask_user child dies after function call
  ask_caption(sv1, sv2, caption);

  /********** write caption to html *********/
  if (cap_html(caption))
    exit(-1);

//////////////// SEND DATA TO NEXT PROCESS ////////////////////

  // so next image can start writing html
  send = index + 1;                 // set send signal that the next (IN INDEX ORDER) img (+1) can write  
#if defined (VERBOSE) || (WAIT)           // this ensures only the next indexed img can write to html
  printf("%d sending %d\n", index, send); // not just the next img that comes through processor
#endif
  if (write(html_out, &send, sizeof(int)) < 0) {
    fprintf(stderr, "error writing bytes out other img process. exiting...\n");
    exit(-1);
  }

  // so next image can start displaying
#if defined (VERBOSE) || (WAIT)
  printf("---%d writing that current img is done with caption to next img\n", index);
#endif
  if (write(to_next, &send, sizeof(int)) < 0) {
    fprintf(stderr, "error writing bytes out other img process. exiting...\n");
    exit(-1);
  }
    
  printf("\n");
//////////////// END OF THIS IMG CONVERSION PROCESS /////////////
  return 0;
}

/* Returns the number of concurrently-running
 * processes with waitpid() and WNOHANG option
 *
 * @param pid[] the array of child processes
 * @param len the len of the array
 * @return the number of currently-alive children
 */
static int concurrent(int pid[], int len) {
  int i, status, num = 0;
  for (i = 0; i < len; i++) {
    if (pid[i] != 0 && waitpid(pid[i], &status, WNOHANG) == 0)
      num++;
  }

  return num;
}

/* Manages all processes, and at the top-level,
 * each singular concurrent image-conversion process.
 * Will exit(-1) from function if error, exit(0) from
 * the child process when a singular image-conversion is completed.
 *
 * @param argc the arg count
 * @param argv the args
 * @return 0
 */
static int process(int argc, char* argv[]) {
  printf("Image Processing will begin now...\n\n");
  
  argc--; // for convenience to get actual number of args
  argv++; // move argv forward too;

  int i, status;
  int pid[argc]; // stores all the pids of its children
  int ptp1[2], ptp2[2];
  
  // create pipe and validate its creation
  if (pipe(ptp1) != 0 || pipe(ptp2) != 0){
    fprintf(stderr, "failed to create a pipe\n");
    return -1;
  }

  for (i = 0; i < argc; i++) {
    int num, max_conversions = 3; // change this number to your liking
    while ((num = concurrent(pid, argc)) >= max_conversions)
      sleep(1); // let the next process run if max running is met
  
    pid[i] = fork();
    if (pid[i] == 0) {      
      char* path = argv[i];
      char* img;
  
      if ((img = strrchr(path, '/')) != NULL)
	img++; // move image past '/' character
      else
	img = path;
      
      // thumb_name = "thumb_photoname", med_name = "med_photoname"
      char* thumb_name = (char*) malloc((strlen("thumb_") + strlen(img) + 1) * sizeof(char));
      char* med_name = (char*) malloc((strlen("med_") + strlen(img) + 1) * sizeof(char));
      
      strcpy(thumb_name, "thumb_");
      strcpy(med_name, "med_");	
      strcat(thumb_name, img);
      strcat(med_name, img);

#ifdef VERBOSE
      printf("begin process on %s\n", path);
#endif
      process_img(path, thumb_name, med_name, i, ptp1, ptp2);
      
      free(thumb_name);
      free(med_name);

      exit(0);
    }
  }
    
  // wait to end main until all children are dead
  // to prevent stdin from closing
  for (i = 0; i < argc; i++)
    waitpid(pid[i], &status, 0);

  printf("=============== END OF PHOTO CONVERSION ===============\n");
  printf("Digital Photo Album is Complete!\n'index.html' album and all edited images are in your current directory.\n");
  return 0;
}

/* The main. Validates args then processes it
 * Program will exit(-1) out early on any error
 * past command-line argument validation
 *
 * @param argc the arg count
 * @param argv the args
 * @return -1 on error, 0 on success
 */
int main(int argc, char* argv[]) {
  if (validate(argc, argv))
    return -1;

  return process(argc, argv);
}
