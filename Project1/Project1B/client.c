#include "head.h"

int RC = 0;

struct option long_options [] = {
	{"log", required_argument, 0, 'l'},
	{"encrypt", no_argument, 0, 'e'},
	{"port", required_argument, 0, 'p'},
	{0, 0, 0, 0}
};

void handle_error(char * msg, int flag);

void write_to_log(FILE* fd, char *c, int received);

void reset_input_mode(void);
void set_input_mode (void);
void sigint_handler(int signum);

void my_exit(int flag);
void clean_exit(int flag);

struct termios saved_attributes;
extern char *optarg;
int socket_fd = 0;
MCRYPT td;
FILE *log_fd = NULL;
int need_encrypy = 0;
char end[] = {0x0D, 0x0A, 0};

void *output_thread() {
	// read from socket
	char buffer;
	int c = 0;
	while (1) {
		c = read(socket_fd, &buffer, 1);

		if (c <= 0) handle_error("", 1);

		write_to_log(log_fd, &buffer, 1);
		if (need_encrypy) {
			mdecrypt_generic(td, &buffer, 1);
		}
		write(STDOUT_FILENO, &buffer, 1);
	}

}

int main(int argc, char** argv) {
	int option_index = 0;
	char * log_file = NULL;
	char buffer;

	int c = 0, s;
	int port = -1;
	

	struct sockaddr_in serv_addr;
	struct hostent *server;

	pthread_attr_t attr;
	pthread_t thread1;

	while ((c = getopt_long(argc, argv, "l:p:e", long_options, &option_index)) != -1) {
		switch (c) {
			case 'l':
				log_file = strdup(optarg);
				break;
			case 'p':
				port = atoi(optarg);
				//fprintf(stderr, "port: %d\n", port);
				break;
			case 'e':
				need_encrypy = 1;
				break;
			case '?':
				break;
		}
	}

	set_input_mode();

	if (log_file != NULL) {
		log_fd = fopen(log_file, "w+");
		if (log_fd == NULL) handle_error("open() log failed", 1);
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

	server = gethostbyname("localhost");
	if (server == NULL) handle_error("gethostbyname() failed", 1);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);

	s = connect(socket_fd, &serv_addr, sizeof(serv_addr));
	if (s < 0) handle_error("connect() failed", 1);

	signal(SIGINT, sigint_handler);

	s = pthread_attr_init(&attr);
	if (s != 0) handle_error("pthread_attr_init() error", 1);
	s = pthread_create(&thread1, &attr, output_thread, NULL);
	if (s != 0) handle_error("pthread_create() error", 1);



	char back;
	while (1) {
		// read keyboard
		c = read(STDIN_FILENO, &buffer, 1);
		if (c < 0) handle_error("read() failed", 1);
		back = buffer;

		if (buffer == 0x0A || buffer == 0x0D) {
			// <cr>  <lf>
			write(STDOUT_FILENO, end, 2);
		} else {
			write(STDOUT_FILENO, &buffer, 1);
		}

		if (need_encrypy) {
			mcrypt_generic(td, &buffer, 1);
		}
		write(socket_fd, &buffer, 1);
		write_to_log(log_fd, &buffer, 0);

		if (back == 0x04) {
			// Ctrl+D
			pthread_cancel(thread1);
			my_exit(0);
		}

		if (back == 0x03) {
			// Ctrl+C
			pthread_cancel(thread1);
			my_exit(0);
		}
	}



	if (log_fd != NULL) fclose(log_fd);
	if (need_encrypy) mcrypt_generic_end(td);
	if (socket_fd > 0) close(socket_fd);

	return 0;

}

void write_to_log(FILE* fd, char *c, int received) {
	if (fd == NULL) return;

	if (received) {
		fprintf(fd, "RECEIVED 1 bytes: %c\n", *c);
	} else {
		fprintf(fd, "SENT 1 bytes:%c\n", *c);
	}

}
void clean_exit(int flag) {
	if (RC != 0) return;
	RC = 1;
	reset_input_mode();
	if (log_fd != NULL) fclose(log_fd);
	if (need_encrypy) mcrypt_generic_end(td);
	if (socket_fd > 0) close(socket_fd);
	exit(flag);
}

void handle_error(char* msg, int flag) {
	if (flag != 0) perror(msg);
	clean_exit(flag);
}

void sigint_handler(int signum) {
	//fprintf(stderr, "%s\n", "sigint");
	clean_exit(1);
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

void my_exit(int flag) {
	if (RC != 0) return;
	RC = 1;
	reset_input_mode();
	if (log_fd != NULL) fclose(log_fd);
	if (need_encrypy) mcrypt_generic_end(td);
	if (socket_fd > 0) close(socket_fd);
	exit(flag);
}


