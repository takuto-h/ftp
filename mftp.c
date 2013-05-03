#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define BUFF_SIZE 1024

static FILE *connect_to_server(const char *host, const char *port);
static void fcopy_from_to(FILE *fromfp, FILE *tofp, size_t nbytes);
static void execute_command(char *input, FILE *ctrlfp, FILE *datafp);

/* remote */
static void execute_exit_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_echo_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rls_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rcd_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rpwd_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_get_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_put_command(char *saveptr, FILE *ctrlfp, FILE *datafp);

/* local */
static void execute_lls_command(char *saveptr);
static void execute_lcd_command(char *saveptr);
static void execute_lpwd_command(char *saveptr);

int
main(int argc, char **argv)
{
        FILE *ctrlfp;
        FILE *datafp;
        char buff[BUFF_SIZE];
        
        if (argc != 4) {
                printf("usage: mftp host ctrlport dataport\n");
                exit(EXIT_FAILURE);
        }
        ctrlfp = connect_to_server(argv[1], argv[2]);
        datafp = connect_to_server(argv[1], argv[3]);
        for (;;) {
                printf("mftp> ");
                if (fgets(buff, BUFF_SIZE, stdin) == NULL) {
                        break;
                }
                execute_command(buff, ctrlfp, datafp);
        }
        execute_exit_command(NULL, ctrlfp, datafp);
        return EXIT_SUCCESS;
}

static FILE *
connect_to_server(const char *host, const char *port)
{
        struct addrinfo hints;
        struct addrinfo *result;
        struct addrinfo *res;
        int eai;
        int sockfd;
        FILE *serverfp;

        hints.ai_flags = AI_NUMERICSERV;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_addrlen = 0;
        hints.ai_addr = NULL;
        hints.ai_canonname = NULL;
        hints.ai_next = NULL;
        eai = getaddrinfo(host, port, &hints, &result);
        if (eai != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(eai));
                exit(EXIT_FAILURE);
        }
        for (res = result; res != NULL; res = res->ai_next) {
                sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (sockfd != -1) {
                        if (connect(sockfd, res->ai_addr, res->ai_addrlen) != -1) {
                                break;
                        }
                        if (close(sockfd) < 0) {
                                perror("close");
                                exit(EXIT_FAILURE);
                        }
                }
        }
        if (res == NULL) {
                fprintf(stderr, "%s:%s: failed to connect\n", host, port);
                exit(EXIT_FAILURE);
        }
        freeaddrinfo(result);
        serverfp = fdopen(sockfd, "r+");
        if (serverfp == NULL) {
                perror("fdopen");
                exit(EXIT_FAILURE);
        }
        return serverfp;
}

static void
fcopy_from_to(FILE *fromfp, FILE *tofp, size_t nbytes)
{
        char buff[BUFF_SIZE];
        
        while (nbytes > BUFF_SIZE) {
                fread(buff, sizeof(char), BUFF_SIZE, fromfp);
                fwrite(buff, sizeof(char), BUFF_SIZE, tofp);
                nbytes -= BUFF_SIZE;
        }
        fread(buff, sizeof(char), nbytes, fromfp);
        fwrite(buff, sizeof(char), nbytes, tofp);
}

static void
execute_command(char *input, FILE *ctrlfp, FILE *datafp)
{
        const char *command;
        char *saveptr;
        
        command = strtok_r(input, " \n", &saveptr);
        if (command == NULL) {
                return;
        }
        if (strcmp(command, "exit") == 0) {
                execute_exit_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "echo") == 0) {
                execute_echo_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "rls") == 0) {
                execute_rls_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "rcd") == 0) {
                execute_rcd_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "rpwd") == 0) {
                execute_rpwd_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "get") == 0) {
                execute_get_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "put") == 0) {
                execute_put_command(saveptr, ctrlfp, datafp);
                return;
        }
        if (strcmp(command, "lls") == 0) {
                execute_lls_command(saveptr);
                return;
        }
        if (strcmp(command, "lcd") == 0) {
                execute_lcd_command(saveptr);
                return;
        }
        if (strcmp(command, "lpwd") == 0) {
                execute_lpwd_command(saveptr);
                return;
        }
        fprintf(stderr, "%s: command not found\n", command);
}

static void
execute_exit_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        (void)saveptr;
        fprintf(ctrlfp, "exit");
        fflush(ctrlfp);
        fclose(ctrlfp);
        fclose(datafp);
        exit(EXIT_SUCCESS);
}

static void
execute_echo_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        size_t nbytes;

        arg = strtok_r(NULL, "\n", &saveptr);
        if (arg == NULL) {
                fprintf(ctrlfp, "echo\n");
        }
        else {
                fprintf(ctrlfp, "echo %s\n", arg);
        }
        fflush(ctrlfp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fcopy_from_to(datafp, stdout, nbytes);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "echo: %s: %s\n", arg, value);
                return;
        }
}

static void
execute_rls_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        size_t nbytes;

        arg = strtok_r(NULL, "\n", &saveptr);
        if (arg == NULL) {
                arg = ".";
        }
        fprintf(ctrlfp, "rls %s\n", arg);
        fflush(ctrlfp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fcopy_from_to(datafp, stdout, nbytes);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "rls: %s: %s\n", arg, value);
                return;
        }
}

static void
execute_rcd_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        size_t nbytes;

        arg = strtok_r(NULL, "\n", &saveptr);
        if (arg == NULL) {
                fprintf(stderr, "rcd: usage: rcd dir\n");
                return;
        }
        fprintf(ctrlfp, "rcd %s\n", arg);
        fflush(ctrlfp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fcopy_from_to(datafp, stdout, nbytes);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "rcd: %s: %s\n", arg, value);
                return;
        }
}

static void
execute_rpwd_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        size_t nbytes;

        fprintf(ctrlfp, "rpwd\n");
        fflush(ctrlfp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fcopy_from_to(datafp, stdout, nbytes);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "rpwd: %s\n", value);
                return;
        }
}

static void
execute_get_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        size_t nbytes;
        FILE *fp;

        arg = strtok_r(NULL, "\n", &saveptr);
        if (arg == NULL) {
                fprintf(stderr, "get: usage: get file\n");
                return;
        }
        if (strrchr(arg, '/') != NULL) {
                fprintf(stderr, "get: %s: cannot use '/'\n", arg);
                return;
        }
        fprintf(ctrlfp, "get %s\n", arg);
        fflush(ctrlfp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fp = fopen(arg, "w");
                if (fp == NULL) {
                        fprintf(stderr, "get: %s: %s\n", arg, strerror(errno));
                        return;
                }
                fcopy_from_to(datafp, fp, nbytes);
                fclose(fp);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "get: %s: %s\n", arg, value);
                return;
        }
}

static void
execute_put_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        FILE *fp;
        struct stat sb;
        size_t nbytes;
        char buff[BUFF_SIZE];
        const char *result;
        const char *value;
        
        arg = strtok_r(NULL, "\n", &saveptr);
        if (arg == NULL) {
                fprintf(stderr, "put: usage: put file\n");
                return;
        }
        if (strrchr(arg, '/') != NULL) {
                fprintf(stderr, "put: %s: cannot use '/'\n", arg);
                return;
        }
        fp = fopen(arg, "r");
        if (fp == NULL) {
                fprintf(stderr, "put: %s: %s\n", arg, strerror(errno));
                return;
        }
        if (fstat(fileno(fp), &sb) < 0) {
                fprintf(stderr, "put: %s: %s\n", arg, strerror(errno));
                fclose(fp);
                return;
        }
        nbytes = sb.st_size;
        fprintf(ctrlfp, "put %s %u\n", arg, nbytes);
        fflush(ctrlfp);
        fcopy_from_to(fp, datafp, nbytes);
        fflush(datafp);
        fclose(fp);
        fgets(buff, BUFF_SIZE, ctrlfp);
        result = strtok_r(buff, " ",  &saveptr);
        value = strtok_r(NULL, "\n", &saveptr);
        if (strcmp(result, "succ:") == 0) {
                nbytes = strtol(value, NULL, 10);
                fcopy_from_to(datafp, stdout, nbytes);
                return;
        }
        if (strcmp(result, "fail:") == 0) {
                fprintf(stderr, "put: %s: %s\n", arg, value);
                return;
        }
}

static void
execute_lls_command(char *saveptr)
{
        const char *dirname;
        struct dirent **direntv;
        int direntc;
        int i;

        dirname = strtok_r(NULL, "\n", &saveptr);
        if (dirname == NULL) {
                dirname = ".";
        }
        direntc = scandir(dirname, &direntv, NULL, alphasort);
        if (direntc < 0) {
                fprintf(stderr, "lls: %s: %s\n", dirname, strerror(errno));
                return;
        }
        for (i = 0; i < direntc; i++) {
                printf("%s\n", direntv[i]->d_name);
                free(direntv[i]);
        }
        free(direntv);
}

static void
execute_lcd_command(char *saveptr)
{
        const char *dirname;

        dirname = strtok_r(NULL, "\n", &saveptr);
        if (dirname == NULL) {
                fprintf(stderr, "lcd: usage: lcd dir\n");
                return;
        }
        if (chdir(dirname) < 0) {
                printf("lcd: %s: %s\n", dirname, strerror(errno));
                return;
        }
}

static void
execute_lpwd_command(char *saveptr)
{
        char buff[BUFF_SIZE];

        (void)saveptr;
        if (getcwd(buff, BUFF_SIZE) == NULL) {
                fprintf(stderr, "lpwd: %s\n", strerror(errno));
                return;
        }
        printf("%s\n", buff);
}
