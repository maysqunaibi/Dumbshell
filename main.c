#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define DEBUG 1
#define MAXLINELEN 4096
#define MAXARGS 128
#define END_OF_LINE 0
#define SEQ_OP ';'
#define SEQUENCE 1
#define MAX_PATH_SIZE 1024
#define MAX_LINE_SIZE 4096


struct cmd
{
    struct cmd *next;
    int terminator;
    char *exe_path;
    int nargs;
    char *arg[MAXARGS];
};

void *ck_malloc(size_t size)
{
    void *ret = malloc(size);
    if (!ret)
    {
        perror("dumbshell:ck_malloc");
        exit(1);
    }
    return ret;
}

char *skip_to_non_ws(char *p)
{
    int ch;
    while (ch = *p)
    {
        if (ch != ' ' && ch != '\t' && ch != '\n')
            return p;
        ++p;
    }
    return 0;
}

char *skip_to_ws_or_sep(char *p)
{
    int ch;
    while (ch = *p)
    {
        if (ch == ' ' || ch == '\t' || ch == '\n')
            return p;
        if (ch == SEQ_OP)
            return p;
        ++p;
    }
    return 0;
}

struct cmd *parse_commands(char *line)
{
    char *ptr;
    int ix;
    struct cmd *cur;

    ptr = skip_to_non_ws(line);
    if (!ptr)
        return 0;
    cur = ck_malloc(sizeof *cur);
    cur->next = 0;
    cur->exe_path = ptr;
    cur->arg[0] = ptr;
    cur->terminator = END_OF_LINE;
    ix = 1;
    for (;;)
    {
        ptr = skip_to_ws_or_sep(ptr);
        if (!ptr)
        {
            break;
        }
        if (*ptr == SEQ_OP)
        {
            *ptr = 0;
            cur->next = parse_commands(ptr + 1);
            if (cur->next)
            {
                cur->terminator = SEQUENCE;
            }
            break;
        }
        *ptr = 0;
        ptr = skip_to_non_ws(ptr + 1);
        if (!ptr)
        {
            break;
        }
        if (*ptr == SEQ_OP)
        {
            /* found a sequence operator */
            cur->next = parse_commands(ptr + 1);
            if (cur->next)
            {
                cur->terminator = SEQUENCE;
            }
            break;
        }
        cur->arg[ix] = ptr;
        ++ix;
    }
    cur->arg[ix] = 0;
    cur->nargs = ix;
    return cur;
}
void execute(struct cmd *clist)
{
    int pid, npid, stat;

    pid = fork();
    if (pid == -1)
    {
        perror("dumbshell:fork");
        exit(1);
    }
    if (!pid)
    {
        /* child */
        execvp(clist->exe_path, clist->arg);
        fprintf(stderr, "No such command: %s\n", clist->exe_path);
        exit(1);
    }
    do
    {
        npid = wait(&stat);
        printf("Process %d exited with status %d\n", npid, stat);
    } while (npid != pid);
    switch (clist->terminator)
    {
    case SEQUENCE:
        execute(clist->next);
    }
}

void free_commands(struct cmd *clist)
{
    struct cmd *nxt;

    do
    {
        nxt = clist->next;
        free(clist);
        clist = nxt;
    } while (clist);
}
/* Function to print Current Directory.*/
// void printDir()
// {
//     char cwd[1024];
//     getcwd(cwd, sizeof(cwd));
//     printf("\nDir: %s \n", cwd);
// }


char *getcwd_(size_t size)
{
    char *pdir = NULL;
    pdir = getcwd(pdir, size);
    if (pdir == NULL)
    {
        perror("getcwd");
    }
    strcat(pdir, "\0");
    return pdir;
}
void ipwd(size_t size)
{
    char *dir = getcwd_(MAX_PATH_SIZE);
    if (dir != NULL)
        printf("%s \n", dir);
}

void icd(char **args)
{
    char des[MAX_PATH_SIZE];
    char newPath[MAX_PATH_SIZE];

    strcpy(newPath, getcwd_(MAX_PATH_SIZE));

    if (args[2] != NULL)
    { /* More than one arg passed */
        fprintf(stderr, "There are too many args\n");
        return;
    }
    else if (!args[1] || (args[1][0] == '~'))
    { /* To change the directory to the /home/user */
        char buf[MAX_PATH_SIZE];
        char desPth[] = "/home/";
        strcpy(buf, getlogin());
        strcat(desPth, buf);
        if (chdir(desPth) == -1)
        {
            perror("chdir");
        }
        return;
    }
    else if (!strcmp(args[1], ".."))
    { /** Change to the parent directory */
        if (chdir("..") == -1)
        {
            perror("chdir");
        }
    }
    else if (args[1][0] != '/')
    { /** If user inputs sub or parent directory of the current directory */
        strcat(newPath, "/");
        strcat(newPath, args[1]);
        if (chdir(newPath) == -1)
        {
            perror("chdir");
        }
    }
    else
    { /** If user inputs a full path directory */
        if (chdir(args[1]) == -1)
        {
            perror("chdir");
        }
    }
}

char *get_command(char *buf, int size, FILE *in)
{
    if (in == stdin)
    {
        fputs("@ ", stdout); /* prompt */
    }
    return fgets(buf, size, in);
}

int main(void)
{
    char linebuf[MAXLINELEN];
    struct cmd *commands;
    char *ch = calloc(sizeof(char), MAX_LINE_SIZE);

    while (get_command(linebuf, MAXLINELEN, stdin) != NULL)
    {
        commands = parse_commands(linebuf);
        ch = get_command(linebuf, MAXLINELEN, stdin);
        if (commands)
        {
            if (!strcmp(ch, "pwd"))
            {
                ipwd(MAX_PATH_SIZE);
                return 1;
            }
            else if (!strcmp(ch, "cd"))
            {
                return 1;
            }
            else
            {
                return 0;
            }
            // execute(commands);
            // free_commands(commands);
        }
    }
}
