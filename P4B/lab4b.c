#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <getopt.h>

#ifdef DUMMY
#define MRAA_GPIO_IN 0
#define MRAA_GPIO_EDGE_RISING 2
typedef int mraa_aio_context;
typedef int mraa_gpio_context;
typedef int mraa_gpio_edge_t;

mraa_aio_context mraa_aio_init(int p)
{
    return 1;
}

int mraa_aio_read(mraa_aio_context c)
{
    return 650;
}

void mraa_aio_close(mraa_aio_context c)
{}

mraa_gpio_context mraa_gpio_init(int p)
{
    return 1;
}

void mraa_gpio_dir(mraa_gpio_context c, int d)
{}

void mraa_gpio_isr(mraa_gpio_context c, mraa_gpio_edge_t e, void(*p)(), void* args)
{}

void mraa_gpio_close(mraa_gpio_context c)
{}

#else
#include <mraa.h>
#endif

#define A0 0
#define GPIO_115 73
#define B 4275
#define R0 100000.0

mraa_aio_context temp;
mraa_gpio_context button;

char scale;
int period, log_fd, sample;

struct timespec cur_ts;
struct tm* cur_tm;
time_t next_time = 0;

float sample_temp()
{
    int reading = mraa_aio_read(temp);
    float R = 1023.0/((float) reading) - 1.0;
    R *= R0;
    float C = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15;
    if (scale == 'F')
        return (C * 9) / 5 + 32;
    else
        return C;
}

void sample_and_print_time()
{
    float temp = sample_temp();
    cur_tm = localtime(&(cur_ts.tv_sec));
    printf("%02d:%02d:%02d %.1f\n", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec, temp);
    if (log_fd)
        dprintf(log_fd, "%02d:%02d:%02d %.1f\n", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec, temp);
    next_time = cur_ts.tv_sec + period;
}

void shutdown_handler()
{
    // outputs and logs a final sample with the time and the string SHUTDOWN (instead of a temperature)
    clock_gettime(CLOCK_REALTIME, &cur_ts);
    float temp = sample_temp();
    cur_tm = localtime(&(cur_ts.tv_sec));
    printf("%02d:%02d:%02d %.1f SHUTDOWN\n", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec, temp);
    if (log_fd)
        dprintf(log_fd, "%02d:%02d:%02d %.1f SHUTDOWN\n", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec, temp);
    exit(0);
}

void parse_stdin(char* input)
{
    if (log_fd)
        dprintf(log_fd, "%s", input);
    
    if (strncmp(input, "STOP", 4) == 0)
    {
        sample = 0;
    }
    else if (strncmp(input, "START", 5) == 0)
    {
        sample = 1;
    }
    else if (strncmp(input, "SCALE=F", 7) == 0)
    {
        scale = 'F';
    }
    else if (strncmp(input, "SCALE=C", 7) == 0)
    {
        scale = 'C';
    }
    else if (strncmp(input, "PERIOD=", sizeof("PERIOD=")) == 0)
    {
        period = atoi(input + sizeof("PERIOD="));
    }
    else if (strncmp(input, "OFF", 3) == 0)
    {
        shutdown_handler();
    }
    else 
    {
        fprintf(stderr, "Error, invalid command\n");
    }
}

int main(int argc, char *argv[])
{
    scale = 'F';
    period = 1;
    sample = 1;
    static struct option long_options[] =
        {
            {"period", required_argument, 0, 'p'},
            {"scale", required_argument, 0, 's'},
            {"log", required_argument, 0, 'l'},
            {0, 0, 0, 0}
        };
    while (1)
    {
        int option_index = 0;
        char c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'p':
            period = atoi(optarg);
            break;
        case 's':
            if (optarg[0] == 'F' || optarg[0] == 'C')
            {
                scale = optarg[0];
                break;
            }
            else 
            {
                fprintf(stderr, "Accepted args for --scale are: C, F\n");
                exit(1);
            }
        case 'l':
            log_fd = creat(optarg, 0666);
            if (!log_fd)
            {
                fprintf(stderr, "Error opening file\n");
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Accepted options are: --period=ARG --scale=ARG --log=ARG\n");
            exit(1);
        }
    }

    temp = mraa_aio_init(A0);
    if (!temp)
    {
        fprintf(stderr, "Error initializing temperature sensor\n");
        exit(1);
    }

    button = mraa_gpio_init(GPIO_115);
    if (!button)
    {
        fprintf(stderr, "Error initializing button\n");
        exit(1);
    }

    mraa_gpio_dir(button, MRAA_GPIO_IN);
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &shutdown_handler, NULL);

    struct pollfd inputfd;
    inputfd.fd = STDIN_FILENO;
    inputfd.events = POLLIN;
    
    char* input = (char*) malloc(256 * sizeof(char));
    if (!input)
    {
        fprintf(stderr, "Error allocating input buffer\n");
        exit(1);
    }

    while (1)
    {
        clock_gettime(CLOCK_REALTIME, &cur_ts);
        if (sample && cur_ts.tv_sec >= next_time)
        {
            sample_and_print_time();
        }

        int p = poll(&inputfd, 1, 0);
        if (p < 0)
        {
            fprintf(stderr, "Error polling from stdin\n");
            exit(1);
        }
        else if (p > 0)
        {
            if (inputfd.revents & POLLIN)
            {
                fgets(input, 256, stdin);
                parse_stdin(input);
            }
            if (inputfd.revents & POLLERR)
            {
                fprintf(stderr, "Error polling from stdin\n");
                exit(1);
            }
        }
    }

    mraa_aio_close(temp);
    mraa_gpio_close(button);

    exit(0);
}