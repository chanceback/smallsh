#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <stddef.h> 
#include <unistd.h> 
#include <ctype.h> 
#include <sys/wait.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>

/* Global Variables and Structs*/
int exit_status = 0;
int recent_pid;

struct parsing_info {
    int background;
    char *outfile;
    char *infile;
};

/* Defined Functions */
void word_splitting(char *line, char **words, char *delim);
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);
void expansion(char *restrict *restrict words);
void parsing(char **words, struct parsing_info *pinfo);
void exit_cmd(int status);
int cd_cmd(char *path);
void builtInFunc(char **words, int *isNonBuiltIn);
void nonBuiltInFunc(char **words, struct parsing_info *pinfo, struct sigaction old_action);
void manage_background(int *childPid, int *childStatus);
// empty function used in signals handling for getline()
void handle_SIGINT(int signo){}


int main(void){
    /* Signals Handling */
    struct sigaction SIGINT_action;
    SIGINT_action.sa_handler = handle_SIGINT;
    struct sigaction ignore_action;
    ignore_action.sa_handler = SIG_IGN;
    struct sigaction old_action;

    sigaction(SIGINT, &ignore_action, &old_action);
    sigaction(SIGTSTP, &ignore_action, NULL); 

    /* Setup */
    // variables used to getline()
    char *line = NULL;
    size_t n = 0;
    // set IFS for word splitting
    char *ifs = getenv("IFS");
    if (ifs == NULL) ifs = " \t\n";

    /* Operating System Loop */
    for(;;) {

        /* Managing Backgroud process takes place here */
        int bgPid, bgChildStatus = 0;
        manage_background(&bgPid, &bgChildStatus);

        /* Prompting */
        char *ps1 = getenv("PS1");
        if (ps1 != NULL) fprintf(stderr,"%s", ps1);
        sigaction(SIGINT, &SIGINT_action, NULL);
        ssize_t line_length = getline(&line, &n, stdin);
        sigaction(SIGINT, &ignore_action, &old_action);
        // exit if getline reaches EOF
        if (line_length == -1){
            if (feof(stdin)) exit_cmd(exit_status);
            clearerr(stdin);
            printf("\n");
            continue;
        }

        /* Word Splitting, Expansion, Parsing */
        char *words[line_length]; // may need to declare this outside of the loop using 512 to prevent memory leak?
        word_splitting(line, words, ifs);
        expansion(words);
        struct parsing_info p1 = {0, NULL, NULL}; // has to be set in every call or else values from previous call remain?
        parsing(words, &p1);

        /* Execution and Waiting */
        // if no input given, do nothing
        if (words[0] == NULL) continue;
        
        // if command is built-in execute
        int isNonBuiltIn = 0;
        builtInFunc(words, &isNonBuiltIn);

        // if not built in attempt to execute non-built-in
        if (isNonBuiltIn) nonBuiltInFunc(words, &p1, old_action);
    }
}


void word_splitting(char *line, char **words, char *delim){

    char *token = strtok(line, delim);
    int indx = 0;

    while (token != NULL){
        words[indx] = strdup(token);
        token = strtok(NULL, delim);
        indx += 1;
    }
    // create a stopping point in the words array
    words[indx++] = NULL;
}

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){

    char *str = *haystack;
    size_t haystack_len = strlen(*haystack);
    size_t const needle_len = strlen(needle),
                 sub_len = strlen(sub);

    for (;(str = strstr(str, needle));){
        ptrdiff_t off = str - *haystack;
        if (sub_len > needle_len){
            str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
            if (!str) goto exit;
            *haystack = str;
            str = *haystack + off;
        }
        memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
        memcpy(str, sub, sub_len);
        haystack_len = haystack_len + sub_len - needle_len;
        str += sub_len;

        if (strcmp(needle, "~/") == 0) break; // prevents multiple instances of ~/ from being replaced
    }
    str = *haystack;
    if (sub_len < needle_len) {
        str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
        if (!str) goto exit;
        *haystack = str;
    }
exit:
    return str;      
}

void expansion(char *restrict *restrict words){

    // Store info in buffers using sprintf
    char home_buff[strlen(getenv("HOME")) + 1];
    sprintf(home_buff, "%s/", getenv("HOME"));

    char pid_buff[50];
    sprintf(pid_buff, "%d", getpid());

    char exit_buff[50];
    sprintf(exit_buff, "%d", exit_status);

    char repid_buff[50];
    if (recent_pid) sprintf(repid_buff, "%d", recent_pid);
    
    // expand the tokens using str_gsub
    int counter = 0;
    while (words[counter] != NULL){
        str_gsub(&words[counter], "~/", home_buff);
        str_gsub(&words[counter], "$$", pid_buff);
        str_gsub(&words[counter], "$?", exit_buff);
        str_gsub(&words[counter], "$!", repid_buff);  
        counter += 1;
    } 
}

void parsing(char **words, struct parsing_info *pinfo){

    int indx = 0;

    // loop to end of array and NULL out comments section if necessary
    while(words[indx] != NULL && words[indx][0] != '#') {
        indx += 1;
    }
    free(words[indx]);
    words[indx] = NULL;

    // check if designated as background process
    if (strcmp(words[indx-1], "&") == 0){
        free(words[indx-1]);
        words[indx-1] = NULL;
        pinfo->background = 1;
        indx -= 1;
    }

    // check for in/out filing
    // somehow does not seem to break if lets say len(words) is only 1?
    if (strcmp(words[indx-2], ">") == 0){
        pinfo->outfile = strdup(words[indx-1]);
        free(words[indx-2]);
        words[indx-2] = NULL;
        indx -= 2;
        if (strcmp(words[indx-2], "<") == 0){
            pinfo->infile = strdup(words[indx-1]);
            free(words[indx-2]);
            words[indx-2] = NULL;
            indx -= 2;
        }
    }
    else if (strcmp(words[indx-2], "<") == 0){
        pinfo->infile = strdup(words[indx-1]);
        free(words[indx-2]);
        words[indx-2] = NULL;
        indx -= 2;
        if (strcmp(words[indx-2], ">") == 0){
            pinfo->outfile = strdup(words[indx-1]);
            free(words[indx-2]);
            words[indx-2] = NULL;
            indx -= 2;
        }
    }     
}

void exit_cmd(int status){

    kill(0, SIGINT);
    fprintf(stderr, "\nexit\n");
    exit(status);
} 

int cd_cmd(char *path){
    //chdir returns -1 if error
    if (chdir(path) == -1) return 1;
    else return 0;
}

void builtInFunc(char **words, int *isNonBuiltIn){

    if (strcmp(words[0], "exit") == 0){
        if (words[1] == NULL){
            exit_cmd(exit_status);
        }
        if (words[2] == NULL){
            for(int i=0; words[1][i] != '\0'; i++){
                if (!isdigit(words[1][i])){
                    fprintf(stderr, "argument given to exit was not integer\n");
                } 
            }
            exit_cmd(atoi(words[1])); // successful exit
        }
        fprintf(stderr, "too many arguments given to exit\n");
    }
    else if (strcmp(words[0], "cd") == 0){
        if (words[1] == NULL){
            if(cd_cmd(getenv("HOME")) == 1) fprintf(stderr, "error changing to home directory\n");
        }
        else if (words[2] == NULL){
            if(cd_cmd(words[1]) == 1) fprintf(stderr, "error changing directories\n");; 
        }
        else fprintf(stderr, "too many arguments given to cd\n");
    }
    else {
        // is a nonbuiltin function or error input
        *isNonBuiltIn = 1;
    }
}

void nonBuiltInFunc(char **words, struct parsing_info *pinfo, struct sigaction old_action){
    pid_t childPid = fork();
    if(childPid == -1){
        perror("fork() failed!");
        exit_cmd(1);
    }
    else if (childPid == 0){
        /* Set signals to default */
        sigaction(SIGINT, &old_action, NULL);
        sigaction(SIGTSTP, &old_action, NULL);

        int outfileError = 0;
        if (pinfo->outfile != NULL){
            int outfile = open(pinfo->outfile, O_WRONLY | O_CREAT, 0777);
            if (outfile == -1){
                perror("Failed to find or create the outfile.\n");
                outfileError = 1;
            }
            if (dup2(outfile, STDOUT_FILENO) == -1){
                perror("Failed to set stdout to outfile.\n");
                outfileError = 1;
            }
            if (outfileError != 0) close(outfile);
        }
        int infileError = 0;
        if (pinfo->infile != NULL){
            int infile = open(pinfo->infile, O_RDONLY);
            if (infile == -1){
                perror("Failed to find infile.\n");
                infileError = 1;
            }
            if (dup2(infile, STDIN_FILENO) == -1){
                perror("Failed to set stdin to infile.\n");
                infileError = 1;
            }
            if (infileError != 0) close(infile);
        }

        if (outfileError == 0 && infileError == 0) execvp(words[0], words);
        perror("exec() failed!");
        exit(1); 
    }
    else {
        int childStatus;
        if (pinfo->background == 0){
            waitpid(childPid, &childStatus, WUNTRACED);
            if (WIFEXITED(childStatus)) exit_status = WEXITSTATUS(childStatus);
            else if (WIFSTOPPED(childStatus)){
                fprintf(stderr, "Child process %d stopped. Continuing.\n", childPid);
                kill(childPid, SIGCONT);
                recent_pid = childPid;
            }
            else if (WIFSIGNALED(childStatus)) exit_status = 128 + WTERMSIG(childStatus);
        } 
        else recent_pid = childPid;
    }       
}

void manage_background(int *childPid, int *childStatus){

    int bgPid;
    int bgStatus;

    bgPid = waitpid(0, &bgStatus, WNOHANG | WUNTRACED);
    if (bgPid > 0){
        if (WIFEXITED(bgStatus)){
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bgPid, WEXITSTATUS(bgStatus));
        }
        else if (WIFSTOPPED(bgStatus)){
            fprintf(stderr, "Child process %d stopped. Continuing.\n", bgPid);
            kill(bgPid, SIGCONT);
        }
        else if (WIFSIGNALED(bgStatus)){
            fprintf(stderr, "Child process %d done. Signaled %d. \n", bgPid, WTERMSIG(bgStatus));
        }
        *childPid = bgPid;
        *childStatus = bgStatus;    
    }
}


