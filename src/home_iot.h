#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>

#define SENSOR_PIPE "SENSOR_PIPE"
#define CONSOLE_PIPE "CONSOLE_PIPE"

//----Structs------
typedef struct Sensor_data
{
    char id[50];
    char chave[50];
    int leitura;
    int valor_max;
    int valor_min;
    int media;
    int count;
} Sensor_data;

typedef struct alerta
{
    char id[50];
    char chave[50];
    int valor_max;
    int valor_min;
    int consola;
} alerta;

typedef struct
{
    char cmd[256];
} Comando;

typedef struct
{
    long tipo;
    char cmd[256];
} Mensagem;

//---Variáveis Globais---
// Shared Memoy
Sensor_data *sensores;
alerta *alertas;
int *worker_status;
int *n_chaves;
int shmid;

// Log File
FILE *log_file;

// Unamed Pipes
int (*pipes_p)[2];

// Message Queue's
int mqid; //, mq_externa;

// Internal Queue
Mensagem *inter_queue;
int num_mensagens = 0;

// Semáforo
sem_t *sem_sys;

// Config Var
int queue_sz = 0, n_workers = 0, max_keys = 0, max_sensors = 0, max_alerts = 0;

//Processo Pai
pid_t pai;
// Workers
pid_t **workers;

// Alert Watcher
pid_t watcher;

// Threads
pthread_t console_thread, sensor_thread, dispatcher_thread;

// Variavel de condição
pthread_cond_t *pcond;
pthread_condattr_t attrcond;

// Mutex
pthread_mutex_t *pmutex;
pthread_mutexattr_t attrmutex;

//----Funções----
// Threads
void *console_reader();
void *sensor_reader();
void *dispatcher();

// Sys
void init(char *);
void read_config_file(char *);
void write_log(char *);

void criar_workers();
void criar_threads();
void criar_alert_watcher();

char *executar_comandos(char *, int);

void cleanup();

void alerts_func();
void worker_func(int);
void sensor_func(char *);
