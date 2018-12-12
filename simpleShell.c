/* 
Daniel Bauman
Simple Shell
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

int foregroundOnly = 0;

// Function to catch Ctrl+Z (SIGTSTP) and toggle foreground-only mode
void catchSIGTSTP(int signo) {
  char* enterMsg = "Entering foreground-only mode\n";
  char* exitMsg = "Exiting foreground-only mode\n";

  // Enter or exit foreground-only mode
  if (foregroundOnly == 0){
    foregroundOnly = 1;
    write(STDOUT_FILENO, enterMsg, 30);
  } else {
    foregroundOnly = 0;
    write(STDOUT_FILENO, exitMsg, 28);
  }
}

// Replace a substring in a string with a new substring
// Function referenced from https://bit.ly/2M8lFWI
char *replaceSubstring(char *source, char *target, char *new)
{
  static char temp[4096];
  char *ptr;
  ptr = strstr(source, target);
  strncpy(temp, source, ptr-source);
  temp[ptr-source] = '\0';
  sprintf(temp+(ptr-source), "%s%s", new, ptr+strlen(target));
  return temp;
}


void main() {
  // Struct for catching SIGINT
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = SIG_IGN;             // Set handler function
  sigfillset(&SIGINT_action.sa_mask);             // All signals are blocked while sa_handler executes
  SIGINT_action.sa_flags = 0;                     // Set no flags
  sigaction(SIGINT, &SIGINT_action, NULL);

  // Struct for catching SIGTSTP
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action,NULL);

  int numChars = -5;      // How many chars we entered
  size_t bufferSize = 0;  // Holds how large the allocated buffer is
  char* lineIn = NULL;    // Points to a buffer allocated by getline() that holds our entered string + \n + \0

  int argCount = 0;
  char* args[512];
  pid_t spawnpid = -5;
  int childExitMethod = -5;

  int isBgProcess = 0;
  int childPID = 0;
  int exitStatus = 0;
  int termSignal = 0;
  char* fileIn = NULL;
  char* fileOut = NULL;
  int sourceFD, targetFD, result;

  // Array to store child PIDs
  pid_t children[1000];
  int childCount = 0;


 // Main shell loop
 while(1)
  {
    while(1)
    {
      // Get a line of input from the user, then flush stdout
      printf(": "); fflush(stdout);
      numChars = getline(&lineIn, &bufferSize, stdin);

      if (numChars == -1) {
        clearerr(stdin);
      }
      else if (lineIn[0] == '\n'){
        // Check if an empty line was entered; if so, do nothing
      }
      else if (numChars > 2048) {
        printf("Sorry, you may only input up to 2048 characters.\n"); fflush(stdout);
      }
      else {
        // getline() successfully read input, so break out of this loop
        break; 
      }
    }
    
    // Separate the input string into individual arguments using strtok(),
    // and store them in the args array. Then increment argCount and 
    // get the next argument from the input string.

    // Also, check each argument before adding to args array. If it is
    // a <, >, or & symbol, take the next argument to be a file descriptor
    argCount = 0;
    char* args[512] = {0};
    char* arg = strtok(lineIn, " \n");
    while (arg != NULL){
      // Check for < > and & characters
      // If found, do not add as arguments; instead, set the next arguments as file
      // descriptors (< or >) or set the process to be run in the background (&)
      if (strcmp(arg, "<") == 0) {
        // get the next arg and set as stdin
        arg = strtok(NULL, " \n");
        fileIn = strdup(arg);
        // Iterate to next variable
        arg = strtok(NULL, " \n");

      } else if (strcmp(arg, ">") == 0) {
        // get the next arg and set as stdout
        arg = strtok(NULL, " \n");
        fileOut = strdup(arg);
        // Iterate to next variable
        arg = strtok(NULL, " \n");

      } else if (strcmp(arg, "&") == 0){
        // Set to BG process if foreground-only mode isn't enabled
        if(foregroundOnly == 0){
          isBgProcess = 1;
          break;
        } else {
          break;
        }

      } else {
        // Check for '$$' in the arg string; if found, replace with PID
        if(strstr(arg, "$$") != NULL){
          char* target = "$$";
          char pidStr[20];
          sprintf(pidStr, "%d", getpid());
          arg = strdup(replaceSubstring(arg, "$$", pidStr));
        }

        args[argCount] = strdup(arg); // strcpy() doesn't work here (?), so trying strdup() instead
        argCount++;
        arg = strtok(NULL, " \n");  
      }
    }

    // Use a temp string to check the first character in args,
    // because trying to access args[0][0] doesn't work.
    char firstStr[2048];
    strcpy(firstStr, args[0]);

    if (argCount > 512){
      printf("Sorry, no more than 512 arguments allowed.\n"); fflush(stdout);
    } else if (firstStr[0] == '#') {
      // Check if it's a comment; if so, do nothing

    } else if (argCount == 0) {
      // Check if no arguments were passed; if so, do nothing

    } else if (strcmp(args[0], "exit") == 0) {
      // Check if 'exit' was entered; if so, exit()       
      // after killing any existing child processes
      int j;
      for (j=0; j<childCount; j++){
        kill(children[j], SIGKILL);
      }
      exit(0);

    } else if (strcmp(args[0], "status") == 0) {
      // Check if 'status' was entered; if so, print status
      if (WIFEXITED(childExitMethod) != 0){
        printf("exit status is %d\n", WEXITSTATUS(childExitMethod)); fflush(stdout);
      } else if (WIFSIGNALED(childExitMethod) != 0){
        printf("The process was terminated by a signal: %d\n", WTERMSIG(childExitMethod)); fflush(stdout);
      }
      
  
    } else if (strcmp(args[0], "cd") == 0) {
      // Check if 'cd' was entered; if so, change directory
      if (argCount == 1){
        // if no argument was entered, change to home ENV path
        chdir(getenv("HOME"));
      } else if (strcmp(args[1], "..") == 0) {
          chdir(args[1]);
      } else {
          // Otherwise, change to the directory specified
          if(chdir(args[1]) == 0){
            chdir(args[1]);
        } else {
          printf("Error: Directory not found.\n"); fflush(stdout);
        }
      }
    }
      
     else {
      // If we get to this point, we have arguments.
      // Now we can fork a child process to execute the command.
      spawnpid = fork();
      children[childCount] = spawnpid;
      childCount++;
      switch (spawnpid)
      {
        case -1: // Error condition
          printf("Fork error!\n"); fflush(stdout);
          exit(1);
          break;

        case 0: // CHILD PROCESS: run the command w/ arguments

        // First, check if this is a foreground or background process
        // If it is a foreground process, it can be terminated by SIGINT
        // If it is a background process, SIGINT should be ignored
        // Then, do any necessary input/output redirection

        // If this is a BACKGROUND process:
        if (isBgProcess == 1){
          childPID = getpid();

          // (BG Process) Takes stdin from /dev/null OR from a provided input file.
          if (fileIn != NULL){
            sourceFD = open(fileIn, O_RDONLY);
            if (sourceFD == -1) { printf("input file failed to open\n"); fflush(stdout); exit(1); }
            result = dup2(sourceFD, 0);
            if (result == -1) { printf("error redirecting stdin\n"); fflush(stdout); exit(1); }
            // Close the input file
            close(sourceFD);
          } else {
            sourceFD = open("/dev/null", O_RDONLY);
            if (sourceFD == -1) { printf("/dev/null failed to open for reading\n"); fflush(stdout); exit(1); }
            result = dup2(sourceFD, 0);
            if (result == -1) { printf("error redirecting stdin\n"); fflush(stdout); exit(1); }
          }

          // (BG Process) Outputs to /dev/null, OR to a provided output file.
          if (fileOut != NULL){
            targetFD = open(fileOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(targetFD == -1) { printf("output file failed to open\n"); fflush(stdout); exit(1); }
            result = dup2(targetFD, 1);
            if (result == -1) { printf("error redirecting stdout\n"); fflush(stdout); exit(1); }
            // Close the output file
            close(targetFD);
          } else {
            targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (targetFD == -1) { printf("/dev/null failed to open for writing\n"); fflush(stdout); exit(1); }
            result = dup2(targetFD, 1);
            if (result == -1) { printf("error redirecting stdout to /dev/null\n"); fflush(stdout); exit(1); }
          }

        } else {
          // If it is a FOREGROUND process:

          // Set foreground process to catch and ignore Ctrl+C (SIGINT)
          SIGINT_action.sa_handler = SIG_DFL;       // Take the default action for the signal
          sigaction(SIGINT, &SIGINT_action, NULL);  // Activate the struct 

          // A foreground process should redirect stdin IF there was an input file provided
          if (fileIn != NULL){
            sourceFD = open(fileIn, O_RDONLY);
            if (sourceFD == -1) { printf("input file failed to open\n"); fflush(stdout); exit(1); }
            result = dup2(sourceFD, 0);
            if (result == -1) { printf("error redirecting stdin\n"); fflush(stdout); exit(1); }
            // Close the input file
            close(sourceFD);
          } 
          // A foreground process should redirect stdout IF there was an output file provided.
          if (fileOut != NULL){
            targetFD = open(fileOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(targetFD == -1) { printf("output file failed to open\n"); fflush(stdout); exit(1); }
            result = dup2(targetFD, 1);
            if (result == -1) { printf("error redirecting stdout\n"); exit(1); }
            // Close the output file
            close(targetFD);
          }

        }

        // Need to close file stream here?
        


        if (execvp(args[0], args) == -1) {
          // Print error if execv() can't run given command)
          printf("Command could not be executed\n"); fflush(stdout);
          exit(1);

        }

        default: // PARENT PROCESS
          // If the child is a foreground process, wait for the child to return.
          // If the child was killed by a signal, print the signal number
        if (isBgProcess == 0){
          waitpid(spawnpid, &childExitMethod, 0);
          if (WIFSIGNALED(childExitMethod) != 0){
            termSignal = WTERMSIG(childExitMethod);
            printf("Child process %d was terminated by signal %d\n", spawnpid, termSignal); fflush(stdout);
          }
        } else {
          // Print background child PID (printing from within the child messes up prompt)
          printf("Background Child PID: %d\n", spawnpid);
        }
      }
    }
    
    // Check for any returned child processes; if any, print PID and exit/term status
    spawnpid = waitpid(-1, &childExitMethod, WNOHANG);
    
    while (spawnpid > 0){
        if (WIFEXITED(childExitMethod) != 0){
          exitStatus = WEXITSTATUS(childExitMethod);
          printf("Child process %d exited normally with status %d\n", spawnpid, exitStatus); fflush(stdout);
        }
        if (WIFSIGNALED(childExitMethod) != 0){
          termSignal = WTERMSIG(childExitMethod);
          printf("Child process %d was terminated by signal %d\n", spawnpid, termSignal); fflush(stdout);
        }
      spawnpid = waitpid(-1, &childExitMethod, WNOHANG);
    }

    // Reset variables to starting values
    isBgProcess = 0;
    childPID = 0;
    exitStatus = 0;
    termSignal = 0;
    fileIn = NULL;
    fileOut = NULL;

    // Free the memory allocated by getline() or else memory leak
    free(lineIn);
    lineIn = NULL;
  }

}
