#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <ctype.h>

#define PIPE_NAME "SENSOR_PIPE"

int fd, n_mensagens = 0;
sem_t *sem_sensor;
pid_t pai;

typedef struct
{
    char sensor_id[33];
    char chave[33];
    int valor_min;
    int valor_max;
    int intervalo;
    struct Sensor *ponteiro;
} Sensor;

Sensor analisa_comandos(char *, int, char *, int, int);

void envia_dados(Sensor, int);

int init();

void cleanup(int);

void print_mensagens(int);