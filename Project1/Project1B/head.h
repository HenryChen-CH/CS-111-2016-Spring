#define _POSIX_SOURCE
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>

#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>

#include <mcrypt.h>