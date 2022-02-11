/* 
NAME: Joseph Lin
EMAIL: jj.lin42@gmail.com
ID: 505111868
*/

#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include "zlib.h"

#define BUF_SIZE 256

#define READ 0
#define WRITE 1
#define GETATTR 2
#define SETATTR 3
#define POLL 4
#define SOCKET 5
#define CONNECT 6
#define CREAT 7

struct termios orig_t;
z_stream to_server;
z_stream to_client;

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
        case POLL:
            fprintf(stderr, "Error polling: %s\n\r", strerror(errno));
            break;
        case SOCKET:
            fprintf(stderr, "Error creating sockfdet %s\n\r", strerror(errno));
            break;
        case CONNECT:
            fprintf(stderr, "Error connecting to server %s\n\r", strerror(errno));
            break;
        case CREAT:
            fprintf(stderr, "Error creating file: %s\n\r", strerror(errno));
            break;
        default:
            fprintf(stderr, "invalid error code\n\r");
        }
        exit(1);
    }
}

void restore(void)
{
    check_syscall(tcsetattr(STDIN_FILENO, TCSANOW, &orig_t), SETATTR);
}

void cleanup(void)
{
    inflateEnd(&to_client);
    deflateEnd(&to_server);  
}

void terminal_mode()
{
    check_syscall(tcgetattr(STDIN_FILENO, &orig_t), GETATTR);
    struct termios t;
    check_syscall(tcgetattr(STDIN_FILENO, &t), GETATTR);
    t.c_iflag = ISTRIP, t.c_oflag = 0, t.c_lflag = 0;
    check_syscall(tcsetattr(STDIN_FILENO, TCSANOW, &t), SETATTR);
    atexit(restore);
}

int client_connect(unsigned int port)
{
    struct sockaddr_in serv_addr;
    struct hostent* server;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    check_syscall(sockfd, SOCKET);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    server = gethostbyname("localhost");
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));

    check_syscall(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)), CONNECT);

    return sockfd;
}

void init_zlib() 
{
    to_client.zalloc = Z_NULL;
    to_client.zfree = Z_NULL;
    to_client.opaque = Z_NULL;
    if (inflateInit(&to_client) != Z_OK) {
        fprintf(stderr, "Error initializing stream with inflateInit(): %s\r\n", strerror(errno));
        exit(1);
    }
    to_server.zalloc = Z_NULL;
    to_server.zfree = Z_NULL;
    to_server.opaque = Z_NULL;
    if (deflateInit(&to_server, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fprintf(stderr, "Error initializing stream with deflateInit(): %s\r\n", strerror(errno));
        exit(1);
    }
    atexit(cleanup);
}

int main(int argc, char *argv[])
{
    // process arguments
    int option_index;
    int debug_flag = 0, log_flag = 0, compress_flag = 0;
    int port_num = -1;
    int logfd;
    char c;
    static struct option long_options[] =
        {
            {"port", required_argument, 0, 'p'},
            {"log", required_argument, 0, 'l'},
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
        case 'l':
            log_flag = 1;
            logfd = creat(optarg, 0666);
            check_syscall(logfd, CREAT);
            break;
        case 'd':
            debug_flag = 1;
            break;
        case 'c':
            compress_flag = 1;
            init_zlib();
            break;
        default:
            fprintf(stderr, "Accepted usage: ./lab1b_client [--port=PORT_NUM] [--log=FILENAME] [--compress]\n");
            exit(1);
        }
    }

    if (port_num < 0 )
    {
        fprintf(stderr, "Must include port option with the form: ./lab1a_client [--port=PORT_NUM] [--log=FILENAME] [--compress]\n");
        exit(1);
    }

    // switch to no echo non-canonical mode
    terminal_mode();
    // open connection to server
    int sockfd = client_connect(port_num);

    // poll sockfd and stdin
    int p;
    unsigned int i;

    struct pollfd pollfds[] = 
    {
        {STDIN_FILENO, POLLIN, 0},
        {sockfd, POLLIN, 0}
    };

    while (1)
    {
        p = poll(pollfds, 2, -1);
        check_syscall(p, POLL);
        if (p > 0)
        {
            if (pollfds[0].revents & POLLIN) // polling from stdin
            {
                char buf[BUF_SIZE];
                ssize_t bytesRead = read(STDIN_FILENO, &buf, BUF_SIZE);
                check_syscall(bytesRead, READ);

                // write to terminal
                for (i = 0; i < bytesRead; ++i)
                {
                    if (buf[i] == '\r' || buf[i] == '\n')
                    {
                        if (debug_flag)
                            fprintf(stderr, "client received carriage return/line feed from stdin\n\r");
                        check_syscall(write(STDOUT_FILENO, "\r\n", 2), WRITE);
                    }
                    else if (buf[i] == 0x03)
                    { 
                        if (debug_flag)
                            fprintf(stderr, "client received interrupt from stdin\n\r");
                        check_syscall(write(STDOUT_FILENO, "^C", 2), WRITE);
                    }
                    else if (buf[i] == 0x04)
                    {
                        if (debug_flag)
                            fprintf(stderr, "client received eof from stdin\n\r");
                        check_syscall(write(STDOUT_FILENO, "^D", 2), WRITE);
                    }
                    else
                    {
                        if (debug_flag)
                            fprintf(stderr, "client writing to terminal from stdin\n\r");
                        check_syscall(write(STDOUT_FILENO, &buf[i], 1), WRITE);
                    }
                }

                if (!compress_flag) 
                {
                    check_syscall(write(sockfd, buf, bytesRead), WRITE); // send to server by writing to socket
                    if (log_flag)
                    {
                        dprintf(logfd, "SENT %d bytes: ", (int)bytesRead);
                        check_syscall(write(logfd, buf, bytesRead), WRITE);
                        dprintf(logfd, "\n");
                    }
                }
                else
                {        
                    // compress and send to server
                    unsigned char outbuf[1024];
                    to_server.avail_in = bytesRead;
                    to_server.next_in = (unsigned char *) buf;
                    to_server.avail_out = 1024;
                    to_server.next_out = (unsigned char *) outbuf;
                    do
                    {
                        if(deflate(&to_server, Z_SYNC_FLUSH) != Z_OK)
                        {
                            fprintf(stderr, "Error deflating client input: %s\n\r", strerror(errno));
                            exit(1);
                        }
                    } while (to_server.avail_in > 0);

                    size_t compressed_bytes = 1024 - to_server.avail_out;
                    check_syscall(write(sockfd, outbuf, compressed_bytes), WRITE);
                    
                    if (log_flag) 
                    {
                        dprintf(logfd, "SENT %d bytes: ", (int)compressed_bytes);
                        check_syscall(write(logfd, outbuf, compressed_bytes), WRITE);
                        dprintf(logfd, "\n");
                    }
                }                
            }
            if (pollfds[1].revents & POLLIN) // polling from server
            {
                if (debug_flag) fprintf(stderr, "client received input from server\n\r");
                char buf[BUF_SIZE];
                ssize_t bytesRead = read(sockfd, buf, BUF_SIZE);
                if (bytesRead == 0) 
                {
                    if (debug_flag) fprintf(stderr, "no more bytes read from server\n\r");
                    exit(0);
                }
                check_syscall(bytesRead, READ);
                if (!compress_flag) 
                {
                    check_syscall(write(STDOUT_FILENO, buf, bytesRead), WRITE); // write to terminal

                    if (log_flag) 
                    {
                        dprintf(logfd, "RECEIVED %d bytes: ", (int)bytesRead);
                        check_syscall(write(logfd, buf, bytesRead), WRITE);
                        dprintf(logfd, "\n");
                    }
                }
                else // decompress input from server and then write to terminal
                {
                    unsigned char inbuf[1024];
                    to_client.avail_in = bytesRead;
                    to_client.next_in = (unsigned char *) buf;
                    to_client.avail_out = 1024;
                    to_client.next_out = (unsigned char *) inbuf;
                    do
                    {
                        if (inflate(&to_client, Z_SYNC_FLUSH) != Z_OK)
                        {
                            fprintf(stderr, "Error inflating from server input: %s\n\r", strerror(errno));
                            exit(1);
                        }
                    } while (to_client.avail_in > 0);

                    size_t decompressed_bytes = 1024 - to_client.avail_out;
                    for (i = 0; i < decompressed_bytes; ++i)
                    {
                        if (inbuf[i] == '\n') 
                        {
                            check_syscall(write(STDOUT_FILENO, "\n\r", 2), WRITE);
                        }
                        else
                        {
                            check_syscall(write(STDOUT_FILENO, &inbuf[i], 1), WRITE);
                        }
                    }
                    if (debug_flag) fprintf(stderr, "client decompressing input from server\r\n");

                    if (log_flag) 
                    {
                        dprintf(logfd, "RECEIVED %d bytes: ", (int)bytesRead);
                        check_syscall(write(logfd, buf, bytesRead), WRITE);
                        dprintf(logfd, "\n");
                    }
                }
            }
            if (pollfds[1].revents & (POLLHUP | POLLERR)) {
                shutdown(sockfd);
                if (log_flag) close(logfd);
                exit(0);
            }
        }
    }

    exit(0);
}
