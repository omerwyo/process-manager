#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#define N 16
#define M 5
#define MAX 1000
// Enum that denotes the integer to the corresponding state
typedef enum state {
	READY = 0,
	RUNNING = 1,
	STOPPED = 2,
	TERMINATED = 3,
} State;
/*
a Process consists of a PID and its current state.
We track its priority as its index in the Manager array
*/
typedef struct process {
	pid_t pid;
	// int priority;
	State state;
} Process;

void parent_input_handler(Process manager[], int p[]){
	close(p[1]);

	char input[MAX]

	while(read(p[0], input, MAX) != 0){
		if (read(p[0], input, MAX) > 0){
			input_keyword_handler(manager, input);
		}

		// Check if any processes can makan in 
		// Allow the state of the process manager to reach an equilibrium
		
	}
	close(p[0]);

	// Terminate all processes
}

void input_listener(int p[]){
	close(p[1]);

	char input[MAX];

	while( 1 ){
		char *input = NULL;
		input = readline("cs205> ");
		char *p = strtok(input, " ");
		char *cmd = p;

		write(link[1], cmd)
		int arg_cnt = 0;
		while (p = strtok(NULL, " ")){
			args[arg_cnt] = p;
			arg_cnt++;
		}
	}
}


int main(){
	Process manager[N];
    // char *args[M];
	// char buf[MAX];
	// char msg[MAX];
	int p[2];
    if (pipe(p) < 0) {
        exit(1);
    }
    if (fcntl(p[0], F_SETFL, O_NONBLOCK) < 0) {
        exit(2);
    }

	// Child listens to input, passes it to the parent to handle through the pipe
	// Parent continuously maintains the state of processes
	pid_t pid = fork();

	if (pid > 0){
		input_handler(manager, p);
	} else if (pid == 0){
		input_listener(p);
	} else {
		fprintf(stderr, "fork failed\n")
	}
}