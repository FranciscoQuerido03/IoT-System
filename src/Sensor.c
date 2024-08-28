#include "sensor.h"

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Número de argumentos errado!\n");
        exit(1);
    }

    pai = getpid();

    // Leitura dos parâmetros da linha de comando
    char *sensor_id = argv[1];
    int intervalo = atoi(argv[2]);
    char *key = argv[3];
    int min_value = atoi(argv[4]);
    int max_value = atoi(argv[5]);

    // Variaveis
    Sensor sensor;

    // Handler de sinais para o SIGINT e SIGTSTP
    signal(SIGINT, cleanup);
    signal(SIGTSTP, print_mensagens);

    fd = init();

    sensor = analisa_comandos(sensor_id, intervalo, key, min_value, max_value);

    envia_dados(sensor, fd);

    return 0;
}

int init()
{
    int fd;

    sem_unlink("sem_sensor");
    sem_sensor = sem_open("sem_sensor", O_CREAT | O_EXCL, 0700, 1);
    if (sem_sensor == SEM_FAILED)
    {
        perror("ERRO AO ABRIR MUTEX\n");
        kill(pai, SIGINT);
    }
    else
    {
        printf("SEMÁFORO CRIADO COM SUCESSO!\n");
    }

    // Abre o named pipe para escrita
    if ((fd = open(PIPE_NAME, O_WRONLY)) == -1)
    {
        printf("\nERRO AO ABRIR PIPE!\n\n");
        kill(pai, SIGINT);
    }
    return fd;
}

void envia_dados(Sensor sensor, int fd)
{
    while (1)
    {
        int valor = rand() % (sensor.valor_max - sensor.valor_min + 1) + sensor.valor_min;
        char mensagem[100];
        sprintf(mensagem, "%s#%s#%d", sensor.sensor_id, sensor.chave, valor);
        printf("A enviar mensagem: %s\n", mensagem);
        sem_wait(sem_sensor);
        if (write(fd, mensagem, strlen(mensagem)) == -1)
        {
            printf("ERRO A ESCREVER NO PIPE.\n");
            kill(getegid(), SIGINT);
        }
        sem_post(sem_sensor);
        n_mensagens++;
        sleep(sensor.intervalo);
    }
}

Sensor analisa_comandos(char *sensor_id, int intervalo, char *key, int min_value, int max_value)
{
    Sensor sensor;
    // Validação dos parâmetros
    if (strlen(sensor_id) < 3 || strlen(sensor_id) > 32)
    {
        printf("Identificador do sensor deve ter tamanho mínimo de 3 caracteres e máximo de 32 caracteres.\n");
        kill(pai, SIGINT);
    }

    if (intervalo < 0)
    {
        printf("Intervalo entre envios deve ser maior ou igual a 0.\n");
        kill(pai, SIGINT);
    }

    for (int i = 0; i < strlen(key); i++)
    {
        if (!isalnum(key[i]) && key[i] != '_')
        {
            printf("\nFormato inválido para a chave!\n");
            kill(pai, SIGINT);
        }
    }

    if (min_value > max_value)
    {
        printf("Valor mínimo não pode ser maior que valor máximo.\n");
        kill(pai, SIGINT);
    }

    strcpy(sensor.chave, key);
    sensor.intervalo = intervalo;
    strcpy(sensor.sensor_id, sensor_id);
    sensor.valor_max = max_value;
    sensor.valor_min = min_value;
    sensor.ponteiro = NULL;

    printf("Sensor criado com\nSensor id -%s\nIntervalo - %d\nKey - %s\nMin value - %d\nMax_value - %d\n", sensor.sensor_id, sensor.intervalo, sensor.chave, sensor.valor_min, sensor.valor_max);

    return sensor;
}

void print_mensagens(int sig)
{
    printf("Número de mensagens: %d\n", n_mensagens);
}

void cleanup(int sig)
{
    printf("\nSensor a encerrar.\n");
    close(fd);

    // Fechar Semaforo
    sem_close(sem_sensor);
    sem_unlink("sem_sensor");

    printf("\nSensor encerrado com sucesso.\n");
    exit(0);
}
