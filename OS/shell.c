#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

// limits
#define MAX_TOKENS 100
#define MAX_STRING_LEN 100

size_t MAX_LINE_LEN = 10000;

// builtin commands
#define EXIT_STR "exit"
#define EXIT_CMD 0
#define UNKNOWN_CMD 99
#define TRUE 1
#define FALSE 0
#define WRITE_TO_FILE 1
#define READ_FROM_FILE -1
#define NO_REDIRECT 0
#define PERM 0644

FILE *fp; // file struct for stdin
char **tokens;
char *line;

char *commands[100];
int commands_count = 0;

void initialize()
{
    // allocate space for the whole line
    assert((line = malloc(sizeof(char) * MAX_STRING_LEN)) != NULL);

    // allocate space for individual tokens
    assert((tokens = malloc(sizeof(char *) * MAX_TOKENS)) != NULL);

    // open stdin as a file pointer
    assert((fp = fdopen(STDIN_FILENO, "r")) != NULL);
}

int tokenize(char *string)
{
    int token_count = 0;
    int size = MAX_TOKENS;
    char *this_token;

    while ((this_token = strsep(&string, " \t\v\f\n\r")) != NULL)
    {
        if (*this_token == '\0')
            continue;

        tokens[token_count] = this_token;

        printf("Token %d: %s\n", token_count, tokens[token_count]);

        token_count++;

        if (token_count >= size)
        {
            size *= 2;
            // if there are more tokens than space ,reallocate more space
            assert((tokens = realloc(tokens, sizeof(char *) * size)) != NULL);
        }
    }
    return token_count;
}

int read_command()
{
    // getline will reallocate if input exceeds max length
    assert(getline(&line, &MAX_LINE_LEN, fp) > -1);

    printf("Shell read this line: %s\n", line);

    // storing the command and increasing the count
    commands[commands_count++] = strdup(line);

    return tokenize(line);
}

// Function to execute the redirection
void execute_commands(char **tokens, int redirectType, char *fileName)
{

    int fileDes;
    int status;

    if (redirectType == READ_FROM_FILE)
        fileDes = open(fileName, O_RDONLY);
    else if (redirectType == WRITE_TO_FILE)
        fileDes = creat(fileName, PERM);

    switch (fork())
    {
    case -1:
        fprintf(stderr, " fork failed \n");
        exit(1);
        break;

    case 0: // Child process
        if (redirectType == WRITE_TO_FILE)
            dup2(fileDes, STDOUT_FILENO);

        else if (redirectType == READ_FROM_FILE)
            dup2(fileDes, STDIN_FILENO);

        close(fileDes);
        execvp(tokens[0], tokens);
        break;

    default:  // Parent process
        wait(&status);
        close(fileDes);
        break;
    }

}

int run_command(int token_count)
{

    int redirectType = NO_REDIRECT;
    char *fileName = NULL;

    if (strcmp(tokens[0], EXIT_STR) == 0)
        return EXIT_CMD;

    if (strcmp(tokens[0], "!!") == 0)
    {
        char *last_command = commands[commands_count - 2];
        int count = tokenize(last_command);
        return run_command(count);
    }

    if (strcmp(tokens[0], "hist") == 0)
    {
        for (int i = 0; i < commands_count; i++)
            printf("%d . %s \n", i + 1, commands[i]);
        
    }

    int pipePresent = 0;
    int pipeIndex = -1;

    for (int i = 0; i < token_count; i++)
    {
        if (strcmp(tokens[i], ">") == 0)
        {
            redirectType = WRITE_TO_FILE;
            if (i + 1 < token_count)
                fileName = tokens[i + 1];
        }
        else if (strcmp(tokens[i], "<") == 0)
        {
            redirectType = READ_FROM_FILE;
            if (i + 1 < token_count)
                fileName = tokens[i + 1];
        }
        else if (strcmp(tokens[i], "|") == 0)
        {
            // Pipe redirect
            pipePresent = 1;
            pipeIndex = i;
            break;
        }
    }

    if (pipePresent == 1)
    {
        // Create pipes
        int filedes[2];
        if (pipe(filedes) == -1)
        {
            fprintf(stderr, "pipe faield");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, " fork failed \n");
            exit(1);
        }
        else if (pid == 0)  // First Child process
        {
            close(filedes[0]);   // read end of the pipe is closed
            dup2(filedes[1], STDOUT_FILENO);
            close(filedes[1]);  // Close the write end of the pipe

            // Execute the command
            tokens[pipeIndex] = NULL;
            execvp(tokens[0], tokens);
            exit(1);
        }
        else 
        {  // Parent process

            pid_t pid1 = fork();
            if (pid1 < 0)
            {
                fprintf(stderr, " fork failed \n");
                exit(1);
            }
            else if (pid1 == 0) //  Grand Child process
            {
                close(filedes[1]); // write end of the pipe is closed
                dup2(filedes[0], STDIN_FILENO);  // Replacing the standard input with the read end of the pipe
                close(filedes[0]); // read end of the pipe is clsoed

                int tokensRemain = token_count - pipeIndex - 1;

                int i = 0;
                while (i < tokensRemain)
                    tokens[i++] = tokens[pipeIndex + i + 1];
                
                tokens[tokensRemain] = NULL;
                execvp(tokens[0], tokens);
                exit(1);
            }
            else
            {   // Parent process
                // both ends of the pipe are closed
                close(filedes[0]);
                close(filedes[1]);

                // Waiting for both the child processes to terminate
                waitpid(pid, NULL, 0);
                waitpid(pid1, NULL, 0);
            }
        }
    }
    else if (redirectType != NO_REDIRECT && fileName != NULL)
    {
        tokens[token_count - 2] = NULL;
        token_count -= 2;
        execute_commands(tokens, redirectType, fileName);
    }
    else
    {
        int child_status;
        switch (fork())
        {
        case -1:
            fprintf(stderr, "fork failed\n");
            exit(1);
            break;

        case 0:
            // Child process
            tokens[token_count] = NULL;
            execvp(tokens[0], tokens);
            break;

        default:
            // parent process
            // waiting for childProcess to terminate
            wait(&child_status);
            break;
        }
    }

    return UNKNOWN_CMD;
}

int main()
{
    initialize();
    int token_count;
    char **local_tokens = NULL;
    do
    {
        printf("sh550> ");
        token_count = read_command();
    } while (run_command(token_count) != EXIT_CMD);
}
