#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER 1024
#define PATH_MAX 1024
#define WORD_LENGTH_MAX 256

typedef struct _Array{
    int size;
    char** words;
}Array;

typedef struct _Node{
    char* command;
    pid_t pid;
    struct _Node* next;
}Node;

Node* header = NULL;
void nodeAdd(char* command, pid_t pid);
void readCommands(FILE* source);


int isNoFork(char* line){

    //fields
    char* noFork[] = {"cd", "pwd", "exit"};
    int arraySize = ( sizeof(noFork) / sizeof(noFork[0]) );

    //split up words
    char* word = strtok(line, " ");

    int i;
    for(i = 0; i < arraySize; i++){
        if( strcmp(word, noFork[i]) ==  0){

            //cd
            if (i == 0){

                word = strtok(NULL, " ");

                //if optional path passed change to that directory
                if (word != NULL) {

                    //save path
                    char *path = malloc(sizeof(char) * strlen(word) + 1);
                    strcpy(path, word);

                    //check to see if too many args
                    word = strtok(NULL, " ");
                    if (word != NULL) {
                        fprintf(stderr, "cd: too many arguments\n");
                    }

                    else {
                        if (chdir(path) != 0) {
                            free(path);
                            fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
                        }
                    }

                    free(path);
                }

                    //change to curr directory
                else{
                    char buff[PATH_MAX + 1];
                    chdir(getcwd(buff, PATH_MAX + 1));

                }

                return 0;
            }

                //pwd
            else if( i ==1){

                char buff[PATH_MAX + 1];
                getcwd(buff, PATH_MAX + 1);
                printf("%s\n", buff);

                return 0;

            }

                //exit
            else{
                exit(0);
            }
        }
    }

    return 1;
}


void freeList()
{
    Node* tmp;

    while (header != NULL)
    {
        tmp = header;
        header = header->next;
        free(tmp->command);
        free(tmp);
    }

}


int isSpecial(char* word){

    //fields
    char* specialSyntax[] = {">", "<", "&"};
    int arraySize = ( sizeof(specialSyntax) / sizeof(specialSyntax[0]) );

    int i;
    for(i = 0; i < arraySize; i++){
        if( strcmp(word, specialSyntax[i]) ==  0){
            return 0;
        }
    }

    return 1;
}

void handleSpecial(Array* specialChars, int* bgroundFlag){

    //local copies
    char* localInput = malloc(sizeof(char) *WORD_LENGTH_MAX);
    char* localOutput = malloc(sizeof(char) *WORD_LENGTH_MAX);
    int localIn = 0;
    int localOut = 0;

    int i;
    for(i = 0; i < specialChars->size; i++){

        if( strcmp(specialChars->words[i], ">") == 0 ){
            if(i >= specialChars->size -1){
                fprintf(stderr, "No arg passed in for output redirection\n");
                exit(1);
            }
            strcpy(localOutput ,specialChars->words[i+1]);
            i++;
            localOut = 1;
        }

        else if(strcmp(specialChars->words[i], "<") == 0){
            if(i >= specialChars->size -1) {
                fprintf(stderr, "No arg passed in for input redirection\n");
                exit(1);
            }
            localIn = 1;
            strcpy(localInput ,specialChars->words[i+1]);
            i++;
        }

        else if(strcmp(specialChars->words[i], "&") == 0){
            *bgroundFlag = 1;
            i = specialChars->size;

        }
        else{
            fprintf(stderr, "Not a valid command line arg: %s, %s\n", optarg, strerror(errno));
            exit(1);
        }
    }

    if(localOut){
        int outDesc;
        if ((outDesc = open(localOutput, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
            fprintf(stderr, "Could not open file %s for output redirection: %s\n", localOutput, strerror(errno));
        }else{
            dup2(outDesc, STDOUT_FILENO);
            close(outDesc);
        }

    }

    if(localIn){
        FILE *inFile;
        inFile = fopen(localInput, "r");
        if (inFile != NULL) {

            readCommands(inFile);

        }else{
            fprintf(stderr, "Input redirection file invalid");
            exit(1);
        }

    }

    free(localInput);
    free(localOutput);

}

Array* add(Array* array, char* word){

    //no need to realloc if first word
    if(array->size == 0 ){
        array->words[0] = word;
        array->size++;
        return array;
    }

  char** tmp = realloc(array->words, ( sizeof(char*) * (array->size+1) ) );

    //make sure re-allcoc worked
    if(array->words == NULL){
        fprintf(stderr, "Could not allocate more memory:, %s\n", strerror(errno));
        exit(1);
    }


    //add word
    tmp[array->size] = malloc(sizeof(char) * WORD_LENGTH_MAX);
    tmp[array->size] = word;
    //array->words = tmp;

    array->size++;

    return array;
}

int backgroundKill(){

    int status;
    pid_t pid =  waitpid(0, &status, WNOHANG);

    if(pid > 0){

        //find command call
        Node* itr = header;
        while(itr->pid != pid){
            itr = itr->next;
        }
        char* cmdname = itr->command;
        int exstat = WEXITSTATUS(status);
        fprintf(stderr, "[%s (%d) completed with status %d]\n", cmdname, pid, exstat);
        return 1;
    }else{
        return 0;
    }


}



void  execute(char **commands, int bground)
{

    pid_t  pid;
    int status;

    if ((pid = fork()) < 0) {     /* fork a child process           */
        printf("*** ERROR: forking child process failed\n");
        exit(1);
    }
    else if (pid == 0) { /* for the child process:         */
        if (execvp(commands[0], commands) < 0) {     /* execute the command  */
            fprintf(stderr, "%s: %s\n", commands[0], strerror(errno));
            exit(1);
        }
    }
    else{

        //normal wait
        if(bground ==0) {
            while (wait(&status) != pid){
            }
        }else{
            nodeAdd(commands[0], pid);
        }
    }


}

void nodeAdd(char* command, pid_t pid){

    //allocate space for new node and fill fields
    Node* addNode = malloc(sizeof(Node));
    addNode->command = malloc(sizeof(strlen(command))+1);
    strcpy(addNode->command, command);
    addNode->pid = pid;
    addNode->next = NULL;

    if(header == NULL){
        header = addNode;
    }else{

        Node* itr = header;
        while(itr->next != NULL){
            itr = itr->next;
        }

        itr->next = addNode;
    }

}

void readCommands(FILE* source){

    int save_out = dup(STDOUT_FILENO);
    char lineArray[BUFFER];



    while(1) {
        if (source == stdin ) {
            printf("sqysh$ ");
        }else{
            backgroundKill();
        }

        if (fgets(lineArray, BUFFER, source) == NULL) {
            return;
        }

        if (source == stdin) {
            backgroundKill() ;

        }

        //allocate space for array of words and args
        Array *commandWords = malloc(sizeof(Array));
        commandWords->words = malloc(sizeof(char *));
        commandWords->words[0] = malloc(sizeof(char) * WORD_LENGTH_MAX);
        commandWords->size = 0;

        //array for special syntax
        Array *specialSyntax = malloc(sizeof(Array));
        specialSyntax->words = malloc(sizeof(char *));
        specialSyntax->words[0] = malloc(sizeof(char) * WORD_LENGTH_MAX);
        specialSyntax->size = 0;

        //special syntax fields
        int specialFound = 0;
        int bground = 0;
        int *bgroundFlag = &bground;


        //split input into words
        char *line = lineArray;
        strtok(line, "\n");
        char *wordCopy = malloc(sizeof(char) * (strlen(line) + 1));
        strcpy(wordCopy, line);

        //check to see if need to fork
        if (isNoFork(line) != 0) {

            //get command
            char *word = strtok(wordCopy, " ");

            if (isSpecial(word) != 0) {
                commandWords = add(commandWords, word);
            } else {
                specialFound = 1;
                specialSyntax = add(specialSyntax, word);
            }

            //get args or special syntax
            while (word != NULL) {
                word = strtok(NULL, " ");
                if (word != NULL) {

                    //if special dont add, stop parsing
                    if (isSpecial(word) == 0 || specialFound) {
                        specialFound = 1;
                        specialSyntax = add(specialSyntax, word);

                    } else {
                        commandWords = add(commandWords, word);
                    }
                }
            }

            //handle special input
            handleSpecial(specialSyntax, bgroundFlag);


            //add null terminator and execute if non empty
            if (commandWords->size > 0) {
                commandWords = add(commandWords, NULL);
                execute(commandWords->words, bground);
            }

        }

        if (source == stdin) {
            backgroundKill();
        }

        //reset inout and output
        dup2(save_out, STDOUT_FILENO);


        //free mem
        free(commandWords->words);
        free(commandWords);

        free(specialSyntax->words);
        free(specialSyntax);

    }
}



int main(int argc, char* argv[]) {

    //fields
    FILE *inFile;

    //interacitve mode when no command args and terminal attched
    if(argc == 1){
        if(isatty(0)){
            readCommands(stdin);
        }
    }


    else{
        inFile = fopen(argv[1], "r");
        if (inFile != NULL) {

            readCommands(inFile);

        }else{
            fprintf(stderr, "File invalid");
            exit(1);
        }
    }

    freeList();
    fclose(inFile);

    return 0;
}