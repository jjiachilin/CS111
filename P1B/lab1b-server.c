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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "zlib.h"

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
#define SOCKET 9
#define BIND 10
#define LISTEN 11
#define ACCEPT 12

z_stream to_client;
z_stream to_server;

void check_syscall(int ret, int code)
{
    if (ret < 0)
    {
        switch (code)
        {
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
            fprintf(stderr, "Error waiting for pid process: %s\n\r", strerror(errno));
            break;
        case SOCKET:
            fprintf(stderr, "Error initializing socket: %s\n\r", strerror(errno));
            break;
        case BIND:
            fprintf(stderr, "Error binding to IP address: %s\n\r", strerror(errno));
            break;
        case LISTEN:
            fprintf(stderr, "Error listening for connections: %s\n\r", strerror(errno));
            break;
        case ACCEPT:
            fprintf(stderr, "Error accepting connections %s\n\r", strerror(errno));
            break;
        default:
            fprintf(stderr, "invalid error code\n\r");
        }
        exit(1);
    }
}

void cleanup(void)
{
    inflateEnd(&to_server);
    deflateEnd(&to_client);
}

void handler(int signum)
{
    if (signum == SIGPIPE)
    {
        fprintf(stderr, "handling SIGPIPE: %s\n\r", strerror(errno));
        exit(1);
    }
}

int server_connect(unsigned int port_num)
{
    int sockfd, new_fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    unsigned int sin_size;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    check_syscall(sockfd, SOCKET);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_num);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));
    check_syscall(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr)), BIND);
    check_syscall(listen(sockfd, 5), LISTEN);
    sin_size = sizeof(struct sockaddr_in);
    new_fd = accept(sockfd, (struct sockaddr*) &client_addr, &sin_size);
    check_syscall(new_fd, ACCEPT);
    shutdown(sockfd);
    return new_fd;
}

void init_zlib() 
{
    to_client.zalloc = Z_NULL;
    to_client.zfree = Z_NULL;
    to_client.opaque = Z_NULL;
    if (deflateInit(&to_client, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fprintf(stderr, "Error initializing stream with deflateInit(): %s\r\n", strerror(errno));
        exit(1);
    }
    to_server.zalloc = Z_NULL;
    to_server.zfree = Z_NULL;
    to_server.opaque = Z_NULL;
    if (inflateInit(&to_server) != Z_OK) {
        fprintf(stderr, "Error initializing stream with inflateInit(): %s\r\n", strerror(errno));
        exit(1);
    }
    atexit(cleanup);
}

int main(int argc, char *argv[])
{
    // process arguments
    if (argc < 2)
    {
        fprintf(stderr, "Must include port option with the form: ./lab1a_server [--port=PORT_NUM] [--debug]\n");
        exit(1);
    }

    int option_index;
    int port_num = -1;
    int debug_flag = 0, compress_flag = 0;
    char c;

    static struct option long_options[] =
        {
            {"port", required_argument, 0, 'p'},
            {"debug", no_argument, 0, 'd'},
            {"compress", no_argument, 0, 'c'},
            {0, 0, 0, 0}
        };
    while (1)
    {
        option_index = 0;
        c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'p':
            port_num = atoi(optarg);
            break;
        case 'd':
            debug_flag = 1;
            break;
        case 'c':
            compress_flag = 1;
            init_zlib();
            break;
        default:
            fprintf(stderr, "Accepted usage: ./lab1a_server [--port=PORT_NUM] [--compress]\n");
            exit(1);
        }
    }


    // setup server and connect to client
    int sockfd = server_connect(port_num);
    if (debug_flag) fprintf(stderr, "accepted client connection\r\n");

    // fork shell process
    int to_shell[2];
    int to_parent[2];
    signal(SIGPIPE, handler);
    check_syscall(pipe(to_shell), PIPE);
    check_syscall(pipe(to_parent), PIPE);
    pid_t pid = fork();
    check_syscall(pid, FORK);
    if (pid == 0) // child process
    {
        // pipe from parent to shell
        close(to_shell[1]);
        close(STDIN_FILENO);
        dup(to_shell[0]);
        close(to_shell[0]);
        // pipe from stdout, stederr to parent
        close(to_parent[0]);
        close(STDOUT_FILENO);
        dup(to_parent[1]);
        close(STDERR_FILENO);
        dup(to_parent[1]);
        close(to_parent[1]);

        char* filename = "/bin/bash";

        if (debug_flag)
            fprintf(stderr, "executing shell...\n\r");

        if (execlp(filename, filename, (char *)NULL) == -1)
        {
            fprintf(stderr, "Error executing shell: %s\n\r", strerror(errno));
            exit(1);
        }
    }
    else if (pid > 0)
    { // parent process
        close(to_shell[0]);
        close(to_parent[1]);

        int p, status, shutdown = 0;
        unsigned int i;

        struct pollfd pollfds[] = 
            {
                {sockfd, POLLIN, 0},
                {to_parent[0], POLLIN, 0}
            };

        while (1)
        {
            if (waitpid (pid, &status, WNOHANG) != 0) {
				shutdown(sockfd);
				fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", 0x7f & status, WEXITSTATUS(status));
				exit(0);
			}
            p = poll(pollfds, 2, -1);
            check_syscall(p, POLL);
            if (p > 0)
            {
                if (pollfds[0].revents == POLLIN) // input from socket
                {
                    char buf[BUF_SIZE];
                    ssize_t bytesRead = read(sockfd, buf, BUF_SIZE);
                    check_syscall(bytesRead, READ);

                    if (!compress_flag) {
                        for (i = 0; i < bytesRead; ++i)
                        {
                            if (buf[i] == '\r' || buf[i] == '\n') // carriage return/line feed
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received carriage return/line feed from socket\n\r");
                                check_syscall(write(to_shell[1], "\n", 1), WRITE);
                            }
                            else if (buf[i] == 0x03) // control-c is interrupt
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received interrupt from socket\n\r");
                                check_syscall(kill(pid, SIGINT), KILL);
                            }
                            else if (buf[i] == 0x04)
                            { // control-d is eof
                                if (debug_flag)
                                    fprintf(stderr, "server received eof from socket\n\r");
                                close(to_shell[1]);
                                shutdown = 1;
                            }
                            else // forward to shell
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server forwarding to shell from socket\n\r");
                                check_syscall(write(to_shell[1], &buf[i], 1), WRITE);
                            }
                        }
                    }
                    else // decompress input from client and send to shell
                    {
                        char inbuf[1024];
                        to_server.avail_in = bytesRead;
                        to_server.next_in = (unsigned char *) buf;
                        to_server.avail_out = 1024;
                        to_server.next_out = (unsigned char *) inbuf;
                        do
                        {
                            if (inflate(&to_server, Z_SYNC_FLUSH) != Z_OK)
                            {
                                fprintf(stderr, "Error inflating from client input: %s\n\r", strerror(errno));
                                exit(1);
                            }
                        } while (to_server.avail_in > 0);

                        size_t decompressed_bytes = 1024 - to_server.avail_out;

                        for (i = 0; i < decompressed_bytes; ++i) 
                        {
                            if (inbuf[i] == '\r' || inbuf[i] == '\n') // carriage return/line feed
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received carriage return/line feed from socket\n\r");
                                check_syscall(write(to_shell[1], "\n", 1), WRITE);
                            }
                            else if (inbuf[i] == 0x03) // control-c is interrupt
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received interrupt from socket\n\r");
                                check_syscall(kill(pid, SIGINT), KILL);
                            }
                            else if (inbuf[i] == 0x04) // control-d is eof
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received eof from socket\n\r");
                                close(to_shell[1]);
                                shutdown = 1;
                            }
                            else // forward to shell
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server forwarding to shell from socket\n\r");
                                check_syscall(write(to_shell[1], &inbuf[i], 1), WRITE);
                            }
                        }
                    }
                }
                if (pollfds[1].revents & POLLIN) // input from shell process
                { 
                    char buf[BUF_SIZE];
                    ssize_t bytesRead = read(to_parent[0], buf, BUF_SIZE);
                    check_syscall(bytesRead, READ);

                    if (!compress_flag) {
                        for (i = 0; i < bytesRead; ++i)
                        {
                            if (buf[i] == '\n') // carriage return/line feed
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received carriage return/line feed from shell\n\r");
                                check_syscall(write(sockfd, "\r\n", 2), WRITE);
                            }
                            else if (buf[i] == 0x04) // control-d is eof
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server received eof from shell\n\r");
                                shutdown = 1;
                            }
                            else
                            {
                                if (debug_flag)
                                    fprintf(stderr, "server writing to client from shell\n\r");
                                check_syscall(write(sockfd, &buf[i], 1), WRITE);
                            }
                        }
                    }
                    else // compress and write to client
                    {
                        unsigned char outbuf[1024];
                        to_client.avail_in = bytesRead;
                        to_client.next_in = (unsigned char *) buf;
                        to_client.avail_out = 1024;
                        to_client.next_out = (unsigned char *) outbuf;
                        do
                        {
                            if (deflate(&to_client, Z_SYNC_FLUSH) != Z_OK)
                            {
                                fprintf(stderr, "Error deflating shell output: %s\n\r", strerror(errno));
                                exit(1);
                            }
                        } while (to_client.avail_in > 0);
                        check_syscall(write(sockfd, outbuf, 1024 - to_client.avail_out), WRITE);                        
                    }
                }
                if (pollfds[0].revents & (POLLHUP | POLLERR)) shutdown = 1;
            }
            if (shutdown) break;
        }

        shutdown(sockfd);
        check_syscall(waitpid(pid, &status, 0), WAITPID);
        int sig = status & 0x7f;
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", sig, WEXITSTATUS(status));
        exit(0);
    }

    exit(0);
}
