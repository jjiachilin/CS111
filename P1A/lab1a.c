/* 
NAME: Joseph Lin
EMAIL: jj.lin42@gmail.com
ID: 505111868
*/

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>

#define BUF_SIZE 256

#define READ 0
#define WRITE 1
#define GETATTR 2
#define SETATTR 3
#define PIPE 4
#define FORK 5
#define POLL 6
#define KILL 7
#define WAITPID 8

struct termios orig_t;
int to_shell[2];
int to_terminal[2];
pid_t pid;
char* filename;
int debug_flag;

void check_syscall(int ret, int code) {
  if (ret < 0) {
    switch (code) {
      case READ:
        fprintf(stderr, "Error reading: %s\n\r", strerror(errno));
        break;
      case WRITE:
        fprintf(stderr, "Error writing: %s\n\r", strerror(errno));
        break;
      case SETATTR:
        fprintf(stderr, "Error setting terminal attributes: %s\n\r", strerror(errno));
        break;
      case GETATTR:
        fprintf(stderr, "Error getting terminal attributes: %s\n\r", strerror(errno));
        break;
      case PIPE:
        fprintf(stderr, "Error creating pipes: %s\n\r", strerror(errno));
        break;
      case FORK:
        fprintf(stderr, "Error forking process: %s\n\r", strerror(errno));
        break;
      case POLL:
        fprintf(stderr, "Error polling: %s\n\r", strerror(errno));
        break;
      case KILL:
        fprintf(stderr, "Error killing process: %s\n\r", strerror(errno));
        break;
      case WAITPID:
        fprintf(stderr, "Error waiting for pid process %s\n\r", strerror(errno));
        break;
      default:
        fprintf(stderr, "invalid error code\n\r");
    }
    exit(1);
  }
}

void restore(void) {
  check_syscall(tcsetattr(STDIN_FILENO, TCSANOW, &orig_t), SETATTR);
}

void terminal_mode() {
  check_syscall(tcgetattr(STDIN_FILENO, &orig_t), GETATTR);
  struct termios t;
  check_syscall(tcgetattr(STDIN_FILENO, &t), GETATTR);
  t.c_iflag = ISTRIP, t.c_oflag = 0, t.c_lflag = 0;
  check_syscall(tcsetattr(STDIN_FILENO, TCSANOW, &t), SETATTR);
  atexit(restore);
}

void handler(int signum) {
  if (signum == SIGPIPE) {
    fprintf(stderr, "Error, broken pipe: %s\n\r", strerror(errno));
    exit(1);
  }
}

void no_shell_mode(void) {
  ssize_t i, bytesRead;
  char buf[BUF_SIZE];
  char tmpBuf[2];

  while (1) {
    bytesRead = read(STDIN_FILENO, buf, BUF_SIZE);
    check_syscall(bytesRead, READ);
    for(i = 0; i < bytesRead; ++i) {     
      if (buf[i] == 0x4) { // restore and exit if control-d
        tmpBuf[0] = '^';
        tmpBuf[1] = 'D';
        check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);                
        exit(0);
      }
      else if (buf[i] == '\r' || buf[i] == '\n') {
        tmpBuf[0] = '\r';
        tmpBuf[1] = '\n';
        check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);                
      }
      else {
        check_syscall(write(STDOUT_FILENO, &buf[i], 1), WRITE);
      }
    }
  }
}

void shell_mode(void) {
  signal(SIGPIPE, handler);
  check_syscall(pipe(to_shell), PIPE);
  check_syscall(pipe(to_terminal), PIPE);
  pid = fork();
  check_syscall(pid, FORK);
  if (pid == 0) { // child process
    // pipe from terminal to stdin
    close(to_shell[1]);    
    close(STDIN_FILENO);
    dup(to_shell[0]); 
    close(to_shell[0]);

    // pipe from stdout, stederr to terminal
    close(to_terminal[0]); 
    close(STDOUT_FILENO);
    dup(to_terminal[1]);     
    close(STDERR_FILENO);
    dup(to_terminal[1]);
    close(to_terminal[1]);

    if (debug_flag) fprintf(stderr, "executing shell...\n\r");

    if (execlp(filename, filename, (char*) NULL) == -1) {
      fprintf(stderr, "Error executing shell: %s\n\r", strerror(errno));
      exit(1);
    }
  }
  else if (pid > 0) { // parent process
    int p, status, shutdown = 0;
    close(to_shell[0]);
    close(to_terminal[1]);

    struct pollfd pollfds[] = {
      {STDIN_FILENO, POLLIN, 0},
      {to_terminal[0], POLLIN, 0}
    };

    while (1) {
      p = poll(pollfds, 2, -1);
      check_syscall(p, POLL);
      if (p > 0) {
        if (pollfds[0].revents & POLLIN) { // polling from stdin
          char buf[BUF_SIZE];
          char tmpBuf[2];
          ssize_t i;
          ssize_t bytesRead = read(STDIN_FILENO, buf, BUF_SIZE);
          check_syscall(bytesRead, READ);

          for (i = 0; i < bytesRead; ++i) {
            if (buf[i] == '\r' || buf[i] == '\n') { // carriage return/line feed
              if (debug_flag) fprintf(stderr, "received carriage return/line feed from stdin\n\r");
              tmpBuf[0] = '\r';
              tmpBuf[1] = '\n';
              check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);
              check_syscall(write(to_shell[1], tmpBuf+1, 1), WRITE);							 
            }
            else if (buf[i] == 0x03) { // control-c is interrupt
              if (debug_flag) fprintf(stderr, "received interrupt from stdin\n\r");
              tmpBuf[0] = '^';
              tmpBuf[1] = 'C';
              check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);
              check_syscall(kill(pid, SIGINT), KILL);
            }
            else if (buf[i] == 0x04) { // control-d is eof
              if (debug_flag) fprintf(stderr, "received eof from stdin\n\r");
              tmpBuf[0] = '^';
              tmpBuf[1] = 'D';
              check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);
              close(to_shell[1]);
            }
            else { // write to terminal and forward to shell
              if (debug_flag) fprintf(stderr, "writing to terminal and forwarding to shell from stdin\n\r");
              check_syscall(write(STDOUT_FILENO, &buf[i], 1), WRITE);
              check_syscall(write(to_shell[1], &buf[i], 1), WRITE);			
            }
          }
        }
        if (pollfds[1].revents & POLLIN) { // polling from shell
          char buf[BUF_SIZE];
          char tmpBuf[2];
          ssize_t i;
          ssize_t bytesRead = read(to_terminal[0], buf, BUF_SIZE);
          check_syscall(bytesRead, READ);

          for (i = 0; i < bytesRead; ++i) {
            if (buf[i] == '\n') { // carriage return/line feed
              if (debug_flag) fprintf(stderr, "received carriage return/line feed from shell\n\r");
              tmpBuf[0] = '\r';
              tmpBuf[1] = '\n';
              check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);
            }
            else if (buf[i] == 0x04) { // control-d is eof
              if (debug_flag) fprintf(stderr, "received eof from shell\n\r");
              tmpBuf[0] = '^';
              tmpBuf[1] = 'D';
              check_syscall(write(STDOUT_FILENO, tmpBuf, 2), WRITE);
              shutdown = 1;
            }
            else { // write to terminal
              if (debug_flag) fprintf(stderr, "writing to terminal from shell\n\r");
              check_syscall(write(STDOUT_FILENO, &buf[i], 1), WRITE);
            }
          }
        }
        if ((pollfds[0].revents | pollfds[1].revents) & (POLLHUP | POLLERR)) shutdown = 1;
      }

      if (shutdown) break;
    }
    
    check_syscall(waitpid(pid, &status, 0), WAITPID);
    int sig = status & 0x7f;
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", sig, WEXITSTATUS(status));
    exit(0);
  }
}

int main(int argc, char* argv[]) {
  // process arguments
  int shell_flag = 0, option_index; 
  debug_flag = 0;
  char c;
  static struct option long_options[] = 
  {
    {"shell", optional_argument, 0, 's'},
    {"debug", no_argument, 0, 'd'},
    {0, 0, 0, 0}
  };
  while (1) {
    option_index = 0;
    c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == -1) break;
    switch (c) {
      case 's':
        shell_flag = 1;
        if (optarg) filename = optarg;
        else filename = "/bin/bash";
        break;
      case 'd':
        debug_flag = 1;
        break;
      default:
        fprintf(stderr, "Accepted options are: --shell --debug\n");
        exit(1);
    }
  }

  // switch to no echo non-canonical mode
  terminal_mode();
  
  if (shell_flag) shell_mode();
  else no_shell_mode();

  exit(0);
}
