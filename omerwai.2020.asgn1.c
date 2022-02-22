#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>

#define N 16
#define M 5
#define MAXSIZE 1000
#define MAXRUNNING 3

// enum that denotes the integer to the corresponding state
typedef enum state{
	RUNNING = 0,
	READY = 1,
	STOPPED = 2,
	TERMINATED = 3,
	UNUSED = -1
} State;

/*
a Process consists of a PID and its current state.
We track its priority as its index in the processes array
*/
typedef struct process{
	pid_t pid;
	int state;
} Process;

void parent_input_handler(Process processes[], int tunnel[]){
	// close tail before reading
	close(tunnel[1]);

	// To read input that we recieve from the pipe in the child input listener
	char buf[MAXSIZE];

	int temp;

	while ((temp = read(tunnel[0], buf, MAXSIZE)) != 0){
		if (temp != -1){
			input_command_handler(processes, buf);
		}

		// Check if any processes have finished, set the state to terminated.
		// Don't need to SIGKILL them, they're already terminated
		int status;
		pid_t pidCheck = waitpid(-1, &status, WNOHANG);
		if ( pidCheck > 0 ){
			for (int i = 0 ; i < N ; i++){
				if (processes[i].pid == pidCheck){
					processes[i].state = TERMINATED;
				}
			}
		}	

		// Run any processes that can now run as a result of one process finishing
		int runCount = 0;
		int waitingProc = -1;
		// Decreasing priority to iterate until the process with highest priority waiting to be exec'd
		for (int i = N - 1 ; i > 0 ; i--){
			if (processes[i].state == READY){
				waitingProc = i;
			}
			if (processes[i].state == RUNNING){
				runCount++;
			}
		}
		// If space in the running permits and if we have found a suitable process
		if (runCount < MAXRUNNING && waitingProc != -1){
			processes[waitingProc].state = RUNNING;
			kill(processes[waitingProc].pid, SIGCONT);
		}
	}

	// Exit has been issued, time to leave!
	close(tunnel[0]);

	// Terminate all unterminated processes and leave cleanly
	for (int i = 0 ; i < N ; i++){
		if (processes[i].state != TERMINATED && processes[i].pid > 0){
			kill(processes[i].pid, SIGKILL);
			processes[i].state = TERMINATED;
		}
	}

	return;

}

void input_command_handler(Process processes[], char *input){
	// Store arguments
	char *args[M];

	char *p = strtok(input, " \n");
	char *cmd = p;
	int arg_cnt = 0;

	while (p = strtok(NULL, " ")){
		args[arg_cnt] = p;
		arg_cnt++;
	}

	if (strcmp(cmd, "run") == 0)
	{
		pid_t pid = fork();
		if (pid == 0){
			char file[10] = "./";
			strncat(file, args[1], 7);
			char *arg_array[] = {file, NULL, NULL, NULL, NULL};
			for (int i = 1; i < 4; i++){
				arg_array[i] = args[i + 1];
			}
			int execCheck = execvp(arg_array[0], arg_array);
			if (execCheck){
				printf("%s: error in program execution\n", arg_array[0]);
				return;
			}
		}
		else if (pid > 0){
			// Before we push a process into ready state or 
			// let it run if capacity allows, we can conduct the processing here
			int priority = atoi(args[0]);
			int runCount = 0;
			int replacementIndex = -1;

			// Prepare to put this in the array
			processes[priority].pid = pid;

			// Iterate from low priority to high priority
			for (int i = N - 1; i > 0 ; i--){
				if (processes[i].state == RUNNING){
					runCount++;
					// We also check if the priority of our process
					// is higher than the lowest priority currently running
					// so that we can replace that with this one if the limit of 3 is reached
					if (replacementIndex == -1 && i > priority){
						// this only runs once. once we find the process with lowest priority that could be replaced
						replacementIndex = i;
					}
				}
			}
			// Number of processes running is less than 3,
			// allow this incoming one to enter running state
			if (runCount < MAXRUNNING) {
				// Set to running
				processes[priority].state = RUNNING;
			}else {
				// If there are already 3 processes running it 
				// comes down to the priority of the incoming process

				// If incoming process is not high priority enough, just mark as ready first
				if (replacementIndex == -1){
					// Set to ready, and pause the execution of the incoming process
					processes[priority].state = READY;
					kill(pid, SIGSTOP);
				}else {
					// Replace the lower priority running one with the incoming one
					processes[replacementIndex].state = READY;
					processes[priority].state = RUNNING;
					kill(processes[replacementIndex].pid, SIGSTOP);
				}
			}
		}
		else{
			fprintf(stderr, "fork failed\n");
		}
	}
	// SIGTERM, kill a process with pid
	else if (strcmp(cmd, "kill") == 0){  
		pid_t pid = atoi(args[0]);
		bool found = 0;
		// Find the process, kill if found
		for (int i=0; i<N; i++){
		  if (processes[i].pid == pid){
		    found = 1;
			printf("cs205> killing %d\n", pid);
		    kill(processes[i].pid, SIGTERM);
		    processes[i].state = TERMINATED;
		    break;
		  }
		}
		if (!found){
		  printf("pid not found\n");
		}
	}
	// SIGSTOP, stop a process with pid
	else if (strcmp(cmd, "stop") == 0){
		// Code to stop
		pid_t pid = atoi(args[0]);
		int priority = -1;

		for(int i = 0 ; i < N ; i++){
			if (processes[i].pid == pid){
				priority = i;
				processes[i].state = STOPPED;
				printf("cs205> stopping %d\n", pid);
				kill(processes[i].pid, SIGSTOP); 				
			}
		}
		if(priority == -1){
			printf("pid not found\n");
		}

		pid_t readyPID = -1;

		for (int i = 0 ; i < N ; i++){
			// If there are any ready processes, get the highest
			// priority one and let them go in
			if (processes[i].state == READY){
				readyPID = processes[i].pid;
				break;
			}
		}
		if (readyPID != -1){
			kill(readyPID, SIGCONT);
		}
	}

	// SIGCONT, resume a process by passing a pid in
	else if (strcmp(cmd, "resume") == 0){
		pid_t pid = atoi(args[0]);

		// Acts like a flag to find the pid, and also flag out which position in the array its located
		int priority = -1;
		
		for(int i = 0 ; i < N ; i++){
			if (processes[i].pid == pid){
				priority = i;
			}
		}

		// That pid doesn't exist in our records
		if (priority == -1){
			printf("pid not found\n");
			return;
		}

		// Prepare to see if a currently running processed can be subbed out for this incoming one
		int replacementIndex = -1;
		int runCount = 0;

		// Iterate from low priority to high priority
			for (int i = N - 1; i > 0 ; i--){
				if (processes[i].state == RUNNING){
					runCount++;
					// We also check if the priority of our process is higher than the lowest priority currently running
					// so that we can replace that with this one if the limit of 3 is reached
					if (replacementIndex == -1 && i > priority){
						// this only runs once. once we find the process with lowest priority that could be replaced
						replacementIndex = i;
					}
				}
			}
			// Number of processes running is less than 3,
			// allow this readied one to enter running state.
			// dont't worry about replacement index here
			if (runCount < MAXRUNNING) {
				// Set to running
				processes[priority].state = RUNNING;
				kill(pid, SIGCONT);
			} else {
				// If there are already 3 processes running it 
				// comes down to the priority of the incoming process
				if (replacementIndex != -1){
					// Ready the lower priority process, and run the more important process
					processes[replacementIndex].state = READY;
					processes[priority].state = RUNNING;

					kill(pid, SIGCONT);
					kill(processes[replacementIndex].pid, SIGSTOP);
				}
				// If unable to put this in running, place it in ready for later
				else {
					processes[priority].state = READY;
				}
				return;
			}
	}


	// List all processes that are not UNUSED, meaning never seen before
	else if (strcmp(cmd, "list") == 0){
		for (int i = 0 ; i < N; i++){
			if (processes[i].state == UNUSED){
				continue;
			}
			printf("%d, %d, %d\n", processes[i].pid, processes[i].state, i);
		}
	}

	// Bad input
	else{
		printf("invalid command, try again!\n");
	}
	return;
}

void input_listener(int tunnel[]){
	// Close one end
	close(tunnel[0]);

	while (true){
		char *input = NULL;
		input = readline("cs205> ");

		// Handle Exits early, break out and leave
		if (strcmp(input, "exit") == 0){
			printf("exiting, bye!\n");
			break;
		}
		// Pass our input into the tunnel
		write(tunnel[1], input, MAXSIZE);

		// Need to let it sleep for a while to make the output look neater
		sleep(1);
	}
	return;
}

int main(){
	// This is an array of structs, allocated memory for 16 Process structs.
	Process* processes = malloc(N * sizeof *processes );

	// Initialise the states to a default of -1, as specified in our enum 
	// this is to help us later to list only the processes that are relevant
	for(int i = 0 ; i < N ; i++){
		processes[i].state = UNUSED;
	}

	// Set up a (non-blocking) pipe that conveys information from our input listener to our input handler
	int tunnel[2];

	if (pipe(tunnel) < 0){
		exit(1);
	}
	if (fcntl(tunnel[0], F_SETFL, O_NONBLOCK) < 0){
		exit(2);
	}

	// Child listens to input, passes it to the parent to handle through the pipe
	// Parent continuously maintains the state of processes at any given point in time in a while loop
	pid_t pid = fork();

	if (pid > 0){
		parent_input_handler(processes, tunnel);
	}
	else if (pid == 0){
		input_listener(tunnel);
	}
	else{
		fprintf(stderr, "fork failed\n");
	}

	return 0;
}