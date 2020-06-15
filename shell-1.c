#include <assert.h>   // assert
#include <fcntl.h>    // O_RDWR, O_CREAT
#include <stdbool.h>  // bool
#include <stdio.h>    // printf, getline
#include <stdlib.h>   // calloc
#include <string.h>   // strcmp
#include <sys/wait.h> // wait
#include <unistd.h>   // execvp

#define MAX_LINE 80 /* The maximum length command */

//make thing public because I do want to free memory
char* history[MAX_LINE / 2 + 1];
char* tempHistory[MAX_LINE / 2 + 1];
int historylength = 0;
bool Exit = false;

void runCommand(char *args[], bool waitForIt) {
  int child_pid = fork();
  if (child_pid == 0) {
	//sleep(2);
    int success = execvp(args[0], args);
    // only gets here if execvp failed
    //printf("execvp for %s wailed with %d\n", *args, success);
    exit(EXIT_FAILURE);
  }
  if (waitForIt) {
    //printf("Parent waiting on %d\n", child_pid);
    int status;
    int completed_pid = wait(&status);
    //printf("Completed %d with return value %d\n", completed_pid, status);
  }
}

void runPipeCommand(char *args[], char *subargs[], bool waitForIt) {// ls -a || less
	enum { READ, WRITE };
	pid_t pid;
	int pipeFD[2];
	int save1, save2;
	save1 = dup(0);
	save2 = dup(1);

	//level 1
	printf("fork1");
	pid = fork();
	
	if (pid < 0) {
		perror("Error during fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {// Child
		if (pipe(pipeFD) < 0) {
			perror("Error in creating pipe");
			exit(EXIT_FAILURE);
		}
		//level 2
		printf("fork2");
		int pid2 = fork();
		if (pid2 < 0) {
			perror("Error during fork");
			exit(EXIT_FAILURE);
		}
		if (pid2 == 0) {// Grand Child less
			printf("child2\n");
			// child less
			//printf("less\n");
			close(pipeFD[READ]);//close write
			dup2(pipeFD[WRITE], 1);   //stdout is now child's read pipe
			execvp(args[0], args);
		}
		else {//ls -a
			int status;
			int completed_pid = wait(&status);
			printf("parrent2\n");
			close(pipeFD[WRITE]);//close write
			dup2(pipeFD[READ], 0);
			execvp(subargs[0], subargs);
		}
	}
	printf("parrent1\n");
	dup2(save1, 0);
	dup2(save2, 1);
	//exit(EXIT_SUCCESS);
	int status;
	int completed_pid = wait(&status);
}

int readline(char** buffer) {
	size_t len;
	int number_of_chars = getline(buffer, &len, stdin);
	if (number_of_chars > 0) {
		// get rid of \n
		(*buffer)[number_of_chars - 1] = '\0';
	}
	return number_of_chars;
}

int tokenize(char* line, char** tokens) {

	// http://www.cplusplus.com/reference/cstring/strtok/
	char* pch;

	pch = strtok(line, " ");
	int num = 0;
	while (pch != NULL) {
		//printf("Token: %s\n", pch);
		tokens[num] = pch;
		++num;
		pch = strtok(NULL, " ");
	}
	return num;
}

void saveHistory(char *args[], int size) {
	int numberofhistory = 0;

	if (strcmp(args[0], "") != 0)
	{

		int line = 0;
		for (int i = 0; i < (MAX_LINE / 2 ); i++)
		{
			tempHistory[i] = NULL;
		}
		for (int i = 0; i < size; i++) {// if no history call
			if (strcmp(args[i], "!!") != 0) {
				//printf("save1");
				tempHistory[line] = strdup(args[i]);
				line++;
			}
			else {// if history call
				//printf("save2");
				int histroycount = 0;
				while (history[histroycount] != NULL) {
					tempHistory[line] = strdup(history[histroycount]);
					histroycount++;
					line++;
				}
				numberofhistory++;
			}
		}
	}
	//deep copy
	if (size != 0) {
	for (int i = 0; i < MAX_LINE / 2 + 1; i++)
		{
			history[i] = tempHistory[i];
		}
	}
	historylength = size + numberofhistory * historylength - numberofhistory;
}


void run_command(char *args[], int num_of_tokens){
	char** firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));

	for (int i = 0; i <= num_of_tokens; ++i)
		firstcmd[i] = NULL;
	
	int count = 0;
	do {
		//bool waitForC = true;
		int line = 0;
		firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));
		// up to first ";" or "&"
		while (args[count] != NULL && strcmp(args[count], ";") != 0) {
			if (strcmp(args[count], "exit") == 0) {
				//printf("exitrun");
				Exit = true;
				exit(0);
			}
			else if (strcmp(args[count], "!!") == 0) {
				//history_call = true;
				//printf("length %d",historylength);
				for (int i = 0; i < historylength; i++) {
					printf(history[i]);
					printf(" ");
				}
				printf("\n");
				count++;
				if (strcmp(history[0], "") != 0)run_command(history,historylength);
				else printf("No commands in history.\n");
			}
			else if (strcmp(args[count], "&") == 0) {
				//printf("end of run& c:%d",count);
				count++;
				runCommand(firstcmd, false);
				firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));
				line = 0;
				}
			else if (strcmp(args[count], ">") == 0) {
				//printf("open file \n");
				int save = dup(1);
				int fw = open(args[++count], O_RDONLY | O_APPEND | O_WRONLY | O_CREAT);
				dup2(fw, 1);
				//printf("end of run>");
				runCommand(firstcmd, true);
				close(fw);
				dup2(save, 1);
				line = 0;
				firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));
				}
			else if (strcmp(args[count], "<") == 0) {
				//printf("open file \n");
				//char ** buffer = (char **)malloc(MAX_LINE * sizeof(char*));
				count++;
				while (count < num_of_tokens && strcmp(args[count], ";") != 0) {
					int save = dup(0);
					int fw = open(args[count++], O_RDONLY);
					dup2(fw, 0);
					//printf("end of run<");
					runCommand(firstcmd, true);
					close(fw);
					dup2(save, 0);
					line = 0;	
				}
				firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));
				//history_call = true;
				}
			else if (strcmp(args[count], "|") == 0) {
				//printf("open file \n");
				count++;
				char ** seccmd = (char **)malloc(MAX_LINE * sizeof(char*));
				int secline = 0;
				while (args[count] != NULL && strcmp(args[count], ";") != 0) {
					seccmd[secline] = strdup(args[count]);
					printf("read: %s", seccmd[secline]);
					count++;
					secline++;
				}
				//printf("first: %s second: %s", firstcmd[0], seccmd[0]);
				runPipeCommand(firstcmd, seccmd , true);
				firstcmd = (char **)malloc(MAX_LINE * sizeof(char*));
			} 
			else {
				firstcmd[line] = strdup(args[count]);
				++count;
				line++;
			}
		}
		++count;
		runCommand(firstcmd, true);
		//printf("end of run c:%d n:%d", count, num_of_tokens);
	} while (count < num_of_tokens);
	fflush(stdin);
	//printf("start run1");
	//saveHistory(args, num_of_tokens);
	//printf("start run2");
}




int main(void) {
  char *args[MAX_LINE / 2 + 1]; /* command line arguments */
  char* cmdline = (char *)malloc(MAX_LINE * sizeof(char));
       /* flag to determine when to exit program */
  bool waitForC = true;
  history[0] = "";
  while (!Exit) {
	  //printf("history: ");
	  //printf(history[0]);
	  printf("osh> ");
	  //fflush(stdout);

		// tokenize needs a char* it can modify, so need malloc
	  char* command = (char *)malloc(MAX_LINE * sizeof(char));
	  char* sample = (char *)malloc(MAX_LINE * sizeof(char));
	  int len = readline(&sample);
	  strcpy(command, sample);

	  int num_of_tokens = tokenize(command, args);
	  //for (int i = 0; i < num_of_tokens; ++i)
		 // printf("%d. %s\n", i, args[i]);
	  run_command(args,num_of_tokens);
	  saveHistory(args,num_of_tokens);
	  for (int i = 0; i < MAX_LINE / 2 + 1; i++) {
		  args[i] = NULL;
	  }
	}
	
  
  return 0;
}