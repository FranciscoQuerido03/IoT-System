#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>

#define PIPE_NAME "CONSOLE_PIPE"

int num_consola;
pthread_t thread;
int fd, mqid;
char comando[256];
pid_t pai;
sem_t *sem_console;

typedef struct
{
    long tipo;
    char cmd[256];
} Mensagem;

void init();
void *readFromMessageQueue(void *);
void cleanup();
