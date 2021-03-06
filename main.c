#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <sys/wait.h>

#define FORMAT "%5s %s\t%8s %s\n"

#define DEBUG 1
#define MAXLINELEN 4096
#define MAX_PATH_SIZE 1024
#define MAXARGS 128
#define END_OF_LINE 0
#define SEQ_OP ';'
#define SEQUENCE 1


#define NO_SIG_FOUND -2
#define SIGNALS 31

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
/**************************Helper Functions****************************************/
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
        printf("cwd: %s \n", dir);
}
void icd(char **args)
{
    if (args[2] != NULL)
    { /* More than one arg passed */
        fprintf(stderr, "too many arguments! make sure you entered the correct path\n");
        return;
    }
    else if (!args[1] || (args[1][0] == '~'))
    { /* To change the directory to the /home/user */
        char *home = getenv("HOME");
        chdir(home);
        if (chdir(home) == -1)
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
    else
    { /** If user inputs a full path directory */

        if (chdir(args[1]) == -1)
        {
            perror("chdir");
        }
        chdir(args[1]);
    }
}
/**************PS COMMAND********************/
int ips()
{
    DIR *dir;
    struct dirent *ent;
    int i, fd_self, fd;
    unsigned long time, stime;
    char flag, *tty;
    char cmd[256], tty_self[256], path[256], time_s[256];
    FILE *file;

    dir = opendir("/proc");
    fd_self = open("/proc/self/fd/0", O_RDONLY);
    sprintf(tty_self, "%s", ttyname(fd_self));
    printf(FORMAT, "PID", "TTY", "TIME", "CMD");

    while ((ent = readdir(dir)) != NULL)
    {
        flag = 1;
        for (i = 0; ent->d_name[i]; i++)
            if (!isdigit(ent->d_name[i]))
            {
                flag = 0;
                break;
            }

        if (flag)
        {
            sprintf(path, "/proc/%s/fd/0", ent->d_name);
            fd = open(path, O_RDONLY);
            tty = ttyname(fd);

            if (tty && strcmp(tty, tty_self) == 0)
            {

                sprintf(path, "/proc/%s/stat", ent->d_name);
                file = fopen(path, "r");
                fscanf(file, "%d%s%c%c%c", &i, cmd, &flag, &flag, &flag);
                cmd[strlen(cmd) - 1] = '\0';

                for (i = 0; i < 11; i++)
                    fscanf(file, "%lu", &time);
                fscanf(file, "%lu", &stime);
                time = (int)((double)(time + stime) / sysconf(_SC_CLK_TCK));
                sprintf(time_s, "%02lu:%02lu:%02lu",
                        (time / 3600) % 3600, (time / 60) % 60, time % 60);

                printf(FORMAT, ent->d_name, tty + 5, time_s, cmd + 1);
                fclose(file);
            }
            close(fd);
        }
    }
    close(fd_self);
    return 0;
}
/*************KILL COMMAND************/
void killHandler(char **arg) {
    pid_t pid;
    int sig;
    if (!(arg[1])) { /** No arguments passed with the kill command.*/
        printf("\033[33mFew arguments\033[0m\n");
        printf("Try: \nkill pid\nor kill -l or kill -L\nor kill -SIGNAL pid0 pid1 pid2 ...\nor kill -NUMBER pid0 pid1 pid2 ...\n");
    } else if ((!strcmp(arg[1], "-l")) || (!strcmp(arg[1], "-L"))) { /** Print all signals that the shell can send.*/
        printSIGNALS();
    } else if (arg[1] && (arg[2] == NULL)) { /** Send SIGTERM to the pid provided by the user*/
        pid = atoi(arg[1]);
        if (pid == 0) {
            printf("Invalid argument\n  Try kill NUMBER\n");
            return;
        }
        sig = SIGTERM;
        ikill(pid, sig);

    } else { /** Send a signal that user specify to the 1 pid or more, provided by the user.*/
        if ((arg[1])[0] == '-') {
            sig = atoi((arg[1] + 1));
            if (sig == 0) {
                sig = findSig((arg[1] + 1));
                if (sig == NO_SIG_FOUND) {
                    printf("Invalid argument\n  Try -SIGNAL\n");
                }
            }
            if (sig != NO_SIG_FOUND) {
                int index = 2;
                while (arg[index] != NULL) {
                    pid = strtol(arg[index++], NULL, 10);
                    ikill(pid, sig);
                }
            }
        } else {
            printf("Invalid argument\n  Try -SIGNAL\n");
        }
    }
}

void ikill(pid_t pid, int sig) {
    if (kill(pid, sig) == -1) {
        char *temp = calloc(sizeof(char), 64);
        sprintf(temp, "ikill (pid: %d , sig: %d)", pid, sig);
        perror(temp);
        free(temp);
    }
}

int findSig(char *sig) {
    char signals[SIGNALS][10] = {
        {"SIGHUP\0"}, {"SIGINT\0"}, {"SIGQUIT\0"}, {"SIGILL\0"}, {"SIGTRAP\0"}, {"SIGABRT\0"}, {"SIGBUS\0"}, {"SIGFPE\0"}, {"SIGKILL\0"}, {"SIGUSR1\0"}, {"SIGSEGV\0"}, {"SIGUSR2\0"}, {"SIGPIPE\0"}, {"SIGALRM\0"}, {"SIGTERM\0"}, {"SIGSTKFLT\0"}, {"SIGCHLD\0"}, {"SIGCONT\0"}, {"SIGSTOP\0"}, {"SIGTSTP\0"}, {"SIGTTIN\0"}, {"SIGTTOU\0"}, {"SIGURG\0"}, {"SIGXCPU\0"}, {"SIGXFSZ\0"}, {"SIGVTALRM\0"}, {"SIGPROF\0"}, {"SIGWINCH\0"}, {"SIGPOLL\0"}, {"SIGPWR\0"}, {"SIGSYS\0"}};
    for (int i = 0; i < SIGNALS; i++) {
        if (!strcmp(signals[i], sig)) {
            return (i + 1);
        }
    }
    return NO_SIG_FOUND;
}

void printSIGNALS() {
    char signals[SIGNALS][10] = {{"SIGHUP\0"}, {"SIGINT\0"}, {"SIGQUIT\0"}, {"SIGILL\0"}, {"SIGTRAP\0"}, {"SIGABRT\0"}, {"SIGBUS\0"}, {"SIGFPE\0"}, {"SIGKILL\0"}, {"SIGUSR1\0"}, {"SIGSEGV\0"}, {"SIGUSR2\0"}, {"SIGPIPE\0"}, {"SIGALRM\0"}, {"SIGTERM\0"}, {"SIGSTKFLT\0"}, {"SIGCHLD\0"}, {"SIGCONT\0"}, {"SIGSTOP\0"}, {"SIGTSTP\0"}, {"SIGTTIN\0"}, {"SIGTTOU\0"}, {"SIGURG\0"}, {"SIGXCPU\0"}, {"SIGXFSZ\0"}, {"SIGVTALRM\0"}, {"SIGPROF\0"}, {"SIGWINCH\0"}, {"SIGPOLL\0"}, {"SIGPWR\0"}, {"SIGSYS\0"}};
    for (int r = 1; r <= SIGNALS; r++) {
        printf("%2d) %1s\t", r, signals[r - 1]);
        if ((r % 5) == 0)
            printf("\n");
    }
    printf("\n");
}
/*************************************/
void execute(struct cmd *clist)
{
    if (!strcmp(clist->exe_path, "pwd"))
    {
        ipwd(MAX_PATH_SIZE);
        return 1;
    }
    else if (!strcmp(clist->exe_path, "cd"))
    {
        icd(clist->arg);
    }
      else if (!strcmp(clist->exe_path, "ps"))
    {
        ips();
    }
      else if (!strcmp(clist->exe_path, "kill"))
    {
        killHandler(clist->arg);
    }
    // else
    // {

    //     int pid, npid, stat;

    //     pid = fork();
    //     if (pid == -1)
    //     {
    //         perror("dumbshell:fork");
    //         exit(1);
    //     }

    //     if (!pid)
    //     {
    //         char rt[1500];

    //         if (strcmp(clist->exe_path, "pwd") == 0)
    //         {
    //             printf("Current working dir: %s\n", getcwd(rt, sizeof(rt)));
    //         }

    //         if (strcmp(clist->exe_path, "kill") == 0)
    //         {
    //             if (clist->nargs == 5)
    //             {
    //                 kill(atoi(clist->arg[2]), atoi(clist->arg[1]));
    //                 kill(atoi(clist->arg[3]), atoi(clist->arg[1]));
    //                 kill(atoi(clist->arg[4]), atoi(clist->arg[1]));
    //             }
    //             else
    //                 kill(atoi(clist->arg[1]), SIGKILL);
    //             perror("kill");
    //         }

    //         else
    //         {
    //             execvp(clist->exe_path, clist->arg);
    //         }

    //         exit(1);
    //     }
    //     if (strcmp(clist->exe_path, "ps") == 0)
    //     {
    //         DIR *dir;
    //         struct dirent *ent;
    //         int i, fd_self, fd;
    //         unsigned long time, stime;
    //         char flag, *tty;
    //         char cmd[256], tty_self[256], path[256], time_s[256];
    //         FILE *file;

    //         dir = opendir("/proc");
    //         fd_self = open("/proc/self/fd/0", O_RDONLY);
    //         sprintf(tty_self, "%s", ttyname(fd_self));

    //         while ((ent = readdir(dir)) != NULL)
    //         {
    //             flag = 1;
    //             for (i = 0; ent->d_name[i]; i++)
    //                 if (!isdigit(ent->d_name[i]))
    //                 {
    //                     flag = 0;
    //                     break;
    //                 }

    //             if (flag)
    //             {

    //                 fd = open(path, O_RDONLY);
    //                 tty = ttyname(fd);

    //                 if (tty && strcmp(tty, tty_self) == 0)
    //                 {

    //                     sprintf(path, "/proc/%s/stat", ent->d_name);
    //                     file = fopen(path, "r");
    //                     fscanf(file, "%d%s%c%c%c", &i, cmd, &flag, &flag, &flag);
    //                     cmd[strlen(cmd) - 1] = '\0';

    //                     for (i = 0; i < 11; i++)
    //                         fscanf(file, "%lu", &time);
    //                     fscanf(file, "%lu", &stime);
    //                     time = (int)((double)(time + stime) / sysconf(_SC_CLK_TCK));
    //                     sprintf(time_s, "%02lu:%02lu:%02lu",
    //                             (time / 3600) % 3600, (time / 60) % 60, time % 60);

    //                     printf(FORMAT, ent->d_name, tty + 5, time_s, cmd + 1);
    //                     fclose(file);
    //                 }
    //                 close(fd);
    //             }
    //         }
    //         close(fd_self);
    //     }

    //     do
    //     {

    //         npid = wait(&stat);
    //         printf("Process %d exited with status %d\n", npid, stat);
    //     } while (npid != pid);
    // }
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

char *get_command(char *buf, int size, FILE *in)
{
    if (in == stdin)
    {
        char *username = getlogin();
        char *cwd = getcwd_(MAX_PATH_SIZE);
        char *intro[MAX_PATH_SIZE];
        /* printf("username %s", username);*/
        sprintf(intro, "\033[31m%s@\033[35m:\033[35m~%s\033[0m$ ", username, cwd);
        fputs(intro, stdout);
    }
    return fgets(buf, size, in);
}

void main(void)
{
    char linebuf[MAXLINELEN];
    struct cmd *commands;

    while (get_command(linebuf, MAXLINELEN, stdin) != NULL)
    {
        commands = parse_commands(linebuf);
        if (commands)
        {
            execute(commands);
            free_commands(commands);
        }
    }
}