#include "head.h"

struct option long_options [] = {
	{"encrypt", no_argument, 0, 'e'},
	{"port", required_argument, 0, 'p'},
	{0, 0, 0, 0}
};

int RC = 0;

extern char *optarg;
int socket_fd = 0, newsock_fd = 0;
MCRYPT td;
int need_encrypy = 0;

char end[] = {0x0D, 0x0A, 0};
pid_t child_pid;
int to_child_pipe[2];
int from_child_pipe[2];


void handle_error(char * msg, int flag);
void my_exit(int flag);
void sigpipe_handler(int signum);

void *thread_output(void *arg) {
	//fprintf(stderr, "%s\n", "thread start");
	// read from shell and write to socket
	char cache;
	int count = 0;
	int *fd = (int *)arg;
	while (1) {
		count = read(*fd, &cache, 1);
		//fprintf(stderr, "%d\n", count);
		if (count == 0) {
			//fprintf(stderr, "%s\n", "shell EOF");
			my_exit(2);			
		}

		if (count > 0) {
			if (need_encrypy) {
				mcrypt_generic(td, &cache, 1);
			}
			write(newsock_fd, &cache, 1);
		}
	}
}

int main (int argc, char** argv) {
	int option_index = 0;
	char buffer;
	int count = 0;
	int to_child_pipe[2];
	int from_child_pipe[2];

	int port = 0;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	
	pthread_attr_t attr;
	pthread_t thread1;

	int c = 0, s = 0;

	while ((c = getopt_long(argc, argv, "p:e", long_options, &option_index)) != -1) {
		switch (c) {
			case 'p':
				port = atoi(optarg);
				break;
			case 'e':
				need_encrypy = 1;
				break;
			case '?':
				break;
		}
	}

	if (need_encrypy) {
		int key_fd = open("my.key", O_RDONLY);
		int keysize = 16, i;
		char * IV;
		char* key = calloc(1, keysize);
		int c = read(key_fd, key, keysize);
		if (c <= 0) handle_error("read key error or key is tmpty", 1);

		td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
		if (td == MCRYPT_FAILED) handle_error("mcrypt_module_open() falied", 1);

		IV = malloc(mcrypt_enc_get_iv_size(td));
		srand(0);
		for (i = 0; i < mcrypt_enc_get_iv_size(td); i++) { 
			IV[i] = rand(); 
		}

		i = mcrypt_generic_init(td, key, keysize, IV);
		if (i < 0) {
			 mcrypt_perror(i); 
			 my_exit(1); 
		}

	}

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) handle_error("socket() failed", 1);
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		handle_error("bind() failed", 1);
	}
	listen(socket_fd, 1);

	clilen = sizeof(cli_addr);
	newsock_fd = accept(socket_fd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsock_fd < 0) handle_error("accept() failed", 1);


	signal(SIGPIPE, sigpipe_handler);

	dup2(newsock_fd, STDIN_FILENO);
	dup2(newsock_fd, STDOUT_FILENO);
	dup2(newsock_fd, STDERR_FILENO);


	if (pipe(to_child_pipe) == -1) handle_error("pipe() error", 1);
	if (pipe(from_child_pipe) == -1) handle_error("pipe() error", 1);

	child_pid = fork();
	if (child_pid == -1) handle_error("fork() error", 1);

	if (child_pid > 0) {
		// parent process
		close(to_child_pipe[0]);
		close(from_child_pipe[1]);
		s = pthread_attr_init(&attr);
		if (s != 0) handle_error("pthread_attr_init() error", 1);

		s = pthread_create(&thread1, &attr, thread_output, from_child_pipe);
		if (s != 0) handle_error("pthread_create() error", 1);

		while (1) {
			// read from socket and write to shell
			count = read(newsock_fd, &buffer, 1);

			if (count <= 0) {
				//fprintf(stderr, "%s\n", "Read error or EOF");
				kill(SIGHUP, child_pid);
				pthread_cancel(thread1);
				my_exit(1);
			}
			if (count > 0) {
				if (need_encrypy) {
					mdecrypt_generic(td, &buffer, 1);
				}
				if (buffer == 0x0D || buffer == 0x0A) {
					// <cr>  <lf>
					//fprintf(stderr, "%s\n", "server cr lf");
					write(to_child_pipe[1], end+1, 1);
				} else {
					write(to_child_pipe[1], &buffer, 1);
				}
				if (buffer == 0x03 || buffer == 0x04) {
					//fprintf(stderr, "%s\n", "Ctrl+C/D");
					kill(SIGHUP, child_pid);
					pthread_cancel(thread1);
					my_exit(1);
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
		dup2(from_child_pipe[1], STDERR_FILENO);
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

	close(socket_fd);
	mcrypt_generic_end(td);

	return 0;


}


void handle_error(char* msg, int flag) {
	if (flag != 0) perror(msg);

	my_exit(flag);
}


void my_exit(int flag) {
	if (RC != 0) return;
	RC = 1;

	if (need_encrypy) mcrypt_generic_end(td);
	if (socket_fd > 0) close(socket_fd);
	if (newsock_fd > 0) close(newsock_fd);
	
	exit(flag);
}


void sigpipe_handler(int signum) {
	//fprintf(stderr, "%s\n", "SIGPIPE");
	my_exit(2);
}
