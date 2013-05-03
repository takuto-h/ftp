#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define BUFF_SIZE 1024

static int create_acceptable_socket(const char *port);
static FILE *accept_from_client(int acceptfd);
static void provide_service(FILE *ctrlfp, FILE *datafp);
static int fork_and_detach(void);
static void fcopy_from_to(FILE *fromfp, FILE *tofp, size_t nbytes);

/* grandchild */
static void execute_command(char *input, FILE *ctrlfp, FILE *datafp);
static void execute_exit_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_echo_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rls_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rcd_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_rpwd_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_get_command(char *saveptr, FILE *ctrlfp, FILE *datafp);
static void execute_put_command(char *saveptr, FILE *ctrlfp, FILE *datafp);

int
main(int argc, char **argv)
{
        int acceptfd_ctrl;
        int acceptfd_data;
        FILE *ctrlfp;
        FILE *datafp;
        
        if (argc != 3) {
                fprintf(stderr, "usage: mftpd ctrlport dataport\n");
                exit(EXIT_FAILURE);
        }
        acceptfd_ctrl = create_acceptable_socket(argv[1]);
        acceptfd_data = create_acceptable_socket(argv[2]);
        for (;;) {
                ctrlfp = accept_from_client(acceptfd_ctrl);
                datafp = accept_from_client(acceptfd_data);
                provide_service(ctrlfp, datafp);
        }
        if (close(acceptfd_ctrl) < 0) {
                perror("close");
                exit(EXIT_FAILURE);
        }
        if (close(acceptfd_data) < 0) {
                perror("close");
                exit(EXIT_FAILURE);
        }
        return EXIT_SUCCESS;
}

static int
create_acceptable_socket(const char *port)
{
        struct addrinfo hints;
        struct addrinfo *result;
        struct addrinfo *res;
        int eai;
        int sockfd;

        hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_addrlen = 0;
        hints.ai_addr = NULL;
        hints.ai_canonname = NULL;
        hints.ai_next = NULL;
        eai = getaddrinfo(NULL, port, &hints, &result);
        if (eai != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(eai));
                return EXIT_FAILURE;
        }
        for (res = result; res != NULL; res = res->ai_next) {
                sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (sockfd != -1) {
                        if (bind(sockfd, res->ai_addr, res->ai_addrlen) != -1) {
                                break;
                        }
                        if (close(sockfd) < 0) {
                                perror("close");
                                exit(EXIT_FAILURE);
                        }
                }
        }
        if (res == NULL) {
                fprintf(stderr, "%s: failed to bind\n", port);
                exit(EXIT_FAILURE);
        }
        freeaddrinfo(result);
        if (listen(sockfd, SOMAXCONN) < 0) {
                perror("listen");
                exit(EXIT_FAILURE);
        }
        return sockfd;
}

static FILE *
accept_from_client(int acceptfd)
{
        int clientfd;
        FILE *clientfp;
        
        clientfd = accept(acceptfd, NULL, NULL);
        if (clientfd < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
        }
        clientfp = fdopen(clientfd, "r+");
        if (clientfp == NULL) {
                perror("fdopen");
                exit(EXIT_FAILURE);
        }
        return clientfp;       
}

static void
provide_service(FILE *ctrlfp, FILE *datafp)
{
        char buff[BUFF_SIZE];
        
        if (fork_and_detach() != 0) {
                /* parent */
                return;
        }
        /* grandchild */
        for (;;) {
                if (fgets(buff, BUFF_SIZE, ctrlfp) == NULL) {
                        break;
                }
                execute_command(buff, ctrlfp, datafp);
        }
        fclose(ctrlfp);
        fclose(datafp);
}

static int
fork_and_detach(void)
{
        int childpid;
        int grandchildpid;

        childpid = fork();
        if (childpid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
        }
        if (childpid != 0) {
                /* parent */
                waitpid(childpid, NULL, 0);
                return 1;
        }
        grandchildpid = fork();
        if (grandchildpid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
        }
        if (grandchildpid != 0) {
                /* child */
                _exit(EXIT_SUCCESS);
        }
        /* grandchild */
        return 0;
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
        
        command = strtok_r(input, " \r\n", &saveptr);
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
        fprintf(ctrlfp, "fail: command not found\n");
        fflush(ctrlfp);
}

static void
execute_exit_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        (void)saveptr;
        (void)ctrlfp;
        (void)datafp;
        _exit(EXIT_SUCCESS);
}

static void
execute_echo_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *arg;
        size_t nbytes;

        arg = strtok_r(NULL, "\r\n", &saveptr);
        if (arg == NULL) {
                nbytes = fprintf(datafp, "\n");
        }
        else {
                nbytes = fprintf(datafp, "%s\n", arg);
        }
        fflush(datafp);
        fprintf(ctrlfp, "succ: %u\n", nbytes);
        fflush(ctrlfp);
}

static void
execute_rls_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *dirname;
        struct dirent **direntv;
        int direntc;
        int i;
        size_t nbytes;

        dirname = strtok_r(NULL, "\r\n", &saveptr);
        if (dirname == NULL) {
                dirname = ".";
        }
        direntc = scandir(dirname, &direntv, NULL, alphasort);
        if (direntc < 0) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                return;
        }
        nbytes = 0;
        for (i = 0; i < direntc; i++) {
                nbytes += fprintf(datafp, "%s\n", direntv[i]->d_name);
                free(direntv[i]);
        }
        fflush(datafp);
        fprintf(ctrlfp, "succ: %u\n", nbytes);
        fflush(ctrlfp);
        free(direntv);
}

static void
execute_rcd_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *dirname;

        (void)datafp;
        dirname = strtok_r(NULL, "\r\n", &saveptr);
        if (dirname == NULL) {
                fprintf(ctrlfp, "fail: usage: rcd dir\n");
                fflush(ctrlfp);
                return;
        }
        if (chdir(dirname) < 0) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                return;
        }
        fprintf(ctrlfp, "succ: 0\n");
        fflush(ctrlfp);
}

static void
execute_rpwd_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        char buff[BUFF_SIZE];
        size_t nbytes;

        (void)saveptr;
        if (getcwd(buff, BUFF_SIZE) == NULL) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                return;
        }
        nbytes = fprintf(datafp, "%s\n", buff);
        fflush(datafp);
        fprintf(ctrlfp, "succ: %u\n", nbytes);
        fflush(ctrlfp);
}

static void
execute_get_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *filename;
        FILE *fp;
        struct stat sb;
        size_t nbytes;

        filename = strtok_r(NULL, "\r\n", &saveptr);
        if (filename == NULL) {
                fprintf(ctrlfp, "fail: usage: get file\n");
                fflush(ctrlfp);
                return;
        }
        fp = fopen(filename, "r");
        if (fp == NULL) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                return;
        }
        if (fstat(fileno(fp), &sb) < 0) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                fclose(fp);
                return;
        }
        nbytes = sb.st_size;
        fprintf(ctrlfp, "succ: %u\n", nbytes);
        fflush(ctrlfp);
        fcopy_from_to(fp, datafp, nbytes);
        fflush(datafp);
        fclose(fp);
}

static void
execute_put_command(char *saveptr, FILE *ctrlfp, FILE *datafp)
{
        const char *filename;
        const char *size;
        FILE *fp;
        size_t nbytes;

        filename = strtok_r(NULL, " ", &saveptr);
        size = strtok_r(NULL, "\r\n", &saveptr);
        if (filename == NULL || size == NULL) {
                fprintf(ctrlfp, "fail: usage: put file size\n");
                fflush(ctrlfp);
                return;
        }
        fp = fopen(filename, "w");
        if (fp == NULL) {
                fprintf(ctrlfp, "fail: %s\n", strerror(errno));
                fflush(ctrlfp);
                return;
        }
        nbytes = strtol(size, NULL, 10);
        fcopy_from_to(datafp, fp, nbytes);
        fflush(datafp);
        fprintf(ctrlfp, "succ: 0\n");
        fflush(ctrlfp);
        fclose(fp);
}
