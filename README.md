# README for album.c

### Digital Photo Album

This program allows a user to input a set of raw images, and produce an html photo album, utilizing the ImageMagick Library and its Linux command-line invocations.

For each photo in this input set, the program should:
    
    * Generate a thumbnail version (10%) of the photo
    * Display the thumbnail to the user
    * Ask the user whether or not it should be rotated clockwise or counter-clockwise; if so, do so
    * Ask the user for a caption
    * Generate a properly oriented 25% version of the photo
    * When done, the program should leave the following in the directory in which it was invoked:
      * the thumbnails and medium-size versions
      * a file index.html containing, for each photo:
      	* a properly oriented thumbnail
	* a caption
	* a link from the thumbnail to a properly oriented medium-size version of the photo

### Usage

To build, run `make`.

Run the program using the command-line args:

```bash
./album [img]+
```

To clean up, run `make clean`.

### Sample Input and Output

The `photos/` directory contains image files that you can run the `album` program on. Sample output is contained in `photos_sample_output/`, given the command line invocation when called within the `photos_sample_output/` directory:

```bash
../album ../photos/*.jpg
```

### Notes

The purpose of this project is to practice creating and managing processes, and to show good use of concurrency, good coordination of processes, and an accurate understanding of processes coordination in a lifeline

If you would like verbose step-by-step print statements to track the flow and creation of every process, uncomment the VERBOSE flag at the header of the Makefile. if you would like to see the print statements of the display waiting, as well as the html-order-wait pipeline communication, uncomment the WAIT flag at the header of teh Makefile. Keep in mind, WAIT is a subset of VERBOSE.

`process_lifeline.pdf` shows an example of the lifeline of a single image conversion process' life cycle.