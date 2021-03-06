#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

void signal_handler(int signal) {
  if (signal == SIGSEGV) {
    fprintf(stderr, "Caught segmentation fault\n");
    exit(4);
  }
}

void input_redirection(char* input_name) {
  int ifd = open(input_name, O_RDONLY);
  if (ifd >= 0) {
    close(0);
    dup(ifd);
    close(ifd);
  }
  else {
    fprintf(stderr, "%s: %s\n", input_name, strerror(errno));
    exit(2);
  }
}

void output_redirection(char* output_name) {
  int ofd = creat(output_name, 0666);
  if (ofd >= 0) {
    close(1);
    dup(ofd);
    close(ofd);
  }
  else {
    fprintf(stderr, "%s: %s\n", output_name, strerror(errno));
    exit(3);
  }
}

void cause_segfault() {
  char *p = NULL;
  *p = 'p'; 
}

void stdin_to_stdout() {
  // read 128 bytes at once
  const size_t count = 128;
  char buf[count];
  ssize_t r = read(STDIN_FILENO, buf, count);
  ssize_t w;
  while (r) {
    w = write(STDOUT_FILENO, &buf, r);
    if (w < 0) {
      fprintf(stderr, "Error writing to output: %s", strerror(errno));
      exit(3);
    }
    r = read(STDIN_FILENO, &buf, count);
  }
  if (r < 0) {
    fprintf(stderr, "Error reading from input: %s", strerror(errno));
    exit(2);
  }
}

int main(int argc, char *argv[]) {
  int c;
  int seg_fault_flag = 0;
  char* input_name = NULL;
  char* output_name = NULL;

  while (1) {
    int option_index = 0;
    static struct option long_options[] =
      {
        {"input",     required_argument, 0,           'i'},
        {"output",    required_argument, 0,           'o'},
        {"segfault",  no_argument,       0,           's'},
        {"catch",     no_argument,       0,           'c'},
        {0, 0, 0, 0}
      };
    c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == -1) break;
    switch (c)
      {
      case 's':
        seg_fault_flag = 1;
        break;
      case 'c':
        signal(SIGSEGV, signal_handler);
        break;
      case 'i':
        if (optarg) input_name = optarg;
        break;
      case 'o':
        if (optarg) output_name = optarg;
        break;
      default:
        fprintf(stderr, "Accepted options are: --input=FILE1 --output=FILE2 --segfault --catch\n");
        exit(1);
      }
  }

  // I/O redirection
  if (input_name) input_redirection(input_name);
  if (output_name) output_redirection(output_name);

  // segfault
  if (seg_fault_flag) cause_segfault();

  // read stdin and write to stdout
  stdin_to_stdout();

  return(0);
}
