#define _POSIX_SOURCE
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

#define N_CACHE 1

int RC = 0;

struct option long_options [] = {
	{"shell", no_argument, 0, 's'},
	{0, 0, 0, 0}
};
struct termios saved_attributes;
const char end[] = {0x0D, 0x0A, 0};
pid_t child_pid;

void reset_input_mode(void);
void set_input_mode (void);

void handle_error(char * args);
void sigpipe_handler(int signum);
void sigint_handler(int signum);

void my_exit(int flag);
void handle_error(char * args);

void *thread_output(void *arg) {
	//fprintf(stderr, "%s\n", "thread start");
	char *cache = malloc(N_CACHE*sizeof(char));
	int count = 0;
	int *fd = (int *)arg;
	while (1) {
		count = read(*fd, cache, N_CACHE);
		//fprintf(stderr, "%d\n", count);
		if (count == 0) {
			// EOF
			my_exit(1);			
		}

		if (count > 0) {
			for (int i = 0; i < count; i++) {
				write(STDOUT_FILENO, cache+i, 1);
			}
		}
	}
}




int main(int argc, char** argv) {
	int option_index = 0;
	char buffer[N_CACHE];
	int count = 0;
	int to_child_pipe[2];
	int from_child_pipe[2];
	int shell = 0, s;
	
	pthread_attr_t attr;
	pthread_t thread1;

	int c = 0;
	while ((c = getopt_long(argc, argv, "s", long_options, &option_index)) != -1) {
		if (c == 's') {
			shell = 1;
		}
	}

	if (shell) {
		//fprintf(stderr, "%s\n", "shell");
		if (signal(SIGINT, &sigint_handler) == SIG_ERR) handle_error("signal() error");

		set_input_mode();

		if (signal(SIGPIPE, &sigpipe_handler) == SIG_ERR) handle_error("signal() error");

		if (pipe(to_child_pipe) == -1) handle_error("pipe() error");

		if (pipe(from_child_pipe) == -1) handle_error("pipe() error");

		child_pid = fork();
		if (child_pid == -1) handle_error("fork() error");

		if (child_pid > 0) {
			// parent process
			close(to_child_pipe[0]);
			close(from_child_pipe[1]);
			//create another thread
			s = pthread_attr_init(&attr);
			if (s != 0) handle_error("pthread_attr_init() error");

			s = pthread_create(&thread1, &attr, thread_output, from_child_pipe);
			if (s != 0) handle_error("pthread_create() error");

			// handle input
			while (1) {
				count = read(STDIN_FILENO, buffer, N_CACHE);
				//fprintf(stderr, "%d\n", count);
				if (count > 0) {
					for (int i = 0; i < count; i++) {
						if (buffer[i] == 0x04) {
							// ^D
							kill (child_pid, SIGHUP);
							close(to_child_pipe[1]);
							close(from_child_pipe[0]);
							pthread_cancel(thread1);
							reset_input_mode();
							fprintf(stdout, "%s\n", "shell killed");
							exit(0);
							//my_exit(0);
						} else if (buffer[i] == 0x03) {
							// ^C
							//fprintf(stderr, "%s\n", "Control+C Received");
							kill(child_pid, SIGINT);
							pthread_cancel(thread1);
							my_exit(0);
						} else {
							if (buffer[i] == 0x0A || buffer[i] == 0x0D) {
								write (to_child_pipe[1], "\n", 1);
								write (STDOUT_FILENO, end, 2);
							} else {
								write(STDOUT_FILENO, buffer+i, 1);
								write(to_child_pipe[1], buffer+i, 1);								
							}

						}
					}
				}
			}

		} else {
			// child process
			close(to_child_pipe[1]);
			close(from_child_pipe[0]);
			//redirect input output
			dup2(to_child_pipe[0], STDIN_FILENO);
			dup2(from_child_pipe[1], STDOUT_FILENO);
			close(to_child_pipe[0]);
			close(from_child_pipe[1]);

			char *execvp_argv[2];
			char execvp_filename[] = "/bin/bash";
			execvp_argv[0] = execvp_filename;
			execvp_argv[1] = 0;

			if (execvp(execvp_filename, execvp_argv) == -1) {
				fprintf(stderr, "%s\n", "execvp() error");
				exit(1);
			}
		}

	} else {
		// no shell
		set_input_mode();

		while (1) {
			count = read(STDIN_FILENO, buffer,  N_CACHE);
			for (int i = 0; i < count; i++) {
				if (buffer[i] == 0x0D || buffer[i] == 0x0A) {
					write(STDOUT_FILENO, end, 2);
				} else if (buffer[i] == 0x04 || buffer[i] == 0x03) {
					reset_input_mode();
					exit(0);
				} else {
					write(STDOUT_FILENO, buffer+i, 1);
				}
			}
		}

		return 0;			
	}


}

void reset_input_mode(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);	
}

void set_input_mode (void){

 	struct termios tattr;

  	/* Make sure stdin is a terminal. */
  	if (!isatty(STDIN_FILENO)){
      	fprintf(stderr, "Not a terminal.\n");
      	exit(EXIT_FAILURE);
    }

  	/* Save the terminal attributes so we can restore them later. */
  	tcgetattr(STDIN_FILENO, &saved_attributes);
  	atexit(reset_input_mode);

  	/* Set the funny terminal modes. */
  	tcgetattr(STDIN_FILENO, &tattr);
  	tattr.c_lflag &= ~(ICANON|ECHO|ECHOE); /* Clear ICANON and ECHO. */
  	tattr.c_cc[VMIN] = 1;
  	tattr.c_cc[VTIME] = 0;

  	//tattr.c_lflag &= ~(ISIG);
  	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);

}


void handle_error(char *error) {
	fprintf(stderr, "%s\n", error);
	exit(1);
}

void sigpipe_handler(int signum) {
	//fprintf(stderr, "%s\n", "SIGPIPE");
	int status = 0;
	if (RC == 0) {
		RC = 1;
		reset_input_mode();
		waitpid(child_pid, &status, 0);
		char *tmp = malloc(50*sizeof(char));
		sprintf(tmp, "shell exit: %d\n", status);
		write(STDOUT_FILENO, tmp, strlen(tmp));
		exit(1);
	}	
}

void sigint_handler(int signum) {
	//fprintf(stderr, "%s\n", "Control-C handler");
	kill(child_pid, SIGINT);
	my_exit(1);
}

void my_exit(int flag) {
	if (RC != 0) return;
	RC = 1;
	int status = 0;
	reset_input_mode();

	waitpid(child_pid, &status, 0);
	char * tmp = malloc(50*sizeof(char));
	sprintf(tmp, "shell exit: %d \n", status);
	write(STDOUT_FILENO, tmp, strlen(tmp));
	free(tmp);
	reset_input_mode();
	exit(flag);
}
