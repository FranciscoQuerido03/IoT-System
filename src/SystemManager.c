// Francisco Henrique Baltazar Querido - 2021221158
#include "home_iot.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        write_log("Numero de argumentos errado!\n");
        exit(0);
    }

    // Inicialização
    init(argv[1]);

    //Criar processos e threads
    criar_workers();
    criar_alert_watcher();
    criar_threads();

    signal(SIGINT, cleanup);

    while (1)
    {
    }
}

void init(char *file)
{
    int ctrl = 0;
    //-----------------LOG-------------------
    // Abrir Log file
    if ((log_file = fopen("log.txt", "a")) == NULL)
    {
        perror("Erro ao abrir o log file.\n");
        exit(1);
    }

    write_log("HOME_IOT SIMULATOR STARTING\n");

    // Ler Config File
    read_config_file(file);
    //----------------------------------------------

    //----------------------PIPES----------------------
    unlink(CONSOLE_PIPE);
    unlink(SENSOR_PIPE);
    // SENSOR_PIPE
    if ((ctrl = mkfifo(SENSOR_PIPE, O_CREAT | O_EXCL | 0777)) == -1)
    {
        write_log("ERRO A CRIAR SENSOR_PIPE\n");
        kill(pai, SIGINT);
    }

    // CONSOLE_PIPE
    if ((ctrl = mkfifo(CONSOLE_PIPE, O_CREAT | O_EXCL | 0666)) == -1)
    {
        write_log("ERRO A CRIAR CONSOLE_PIPE\n");
        kill(pai, SIGINT);

    }
    //----------------------------------------------

    //------------Criar Message Queue-------------------
    key_t key = 30;
    if ((mqid = msgget(key, IPC_CREAT | 0777)) == -1)
    {
        write_log("ERRO A CRIAR MESSAGE QUEUE");
        kill(pai, SIGINT);

    }
    //----------------------------------------------

    // Alocar memória para o array de workers
    workers = malloc(n_workers * sizeof(pid_t *));

    //----------------SHARED MEMORY----------------------
    // Criar shared memory
    int espaco = sizeof(Sensor_data) * max_keys + sizeof(int) * n_workers + sizeof(alerta) * max_alerts + sizeof(int) + sizeof(pthread_cond_t) + sizeof(pthread_mutex_t);
    if ((shmid = shmget(IPC_PRIVATE, espaco, IPC_CREAT | 0777)) == -1)
    {
        write_log("ERRO A CRIAR SHARED MEMORY\n");
        kill(pai, SIGINT);

    }

    // Attach shared memory
    if ((sensores = (Sensor_data *)shmat(shmid, NULL, 0)) == (Sensor_data *)-1)
    {
        write_log("ERRO A ATRIBUIR SHARED MEMORY\n");
        kill(pai, SIGINT);

    }

    // Apontar para a memoria alocada para o worker_status e var com nº de keys
    worker_status = (int *)&sensores[max_keys];
    alertas = (alerta *)&worker_status[n_workers];
    n_chaves = (int *)(&alertas[max_alerts]);

    // Cria uma variavel temporaria do tipo sensor_data com os parametros todos a NULL ou 0 para inicializar todos os sensores com esses parametros

    Sensor_data temp;

    temp.id[0] = '\0';
    temp.chave[0] = '\0';
    temp.leitura = 0;
    temp.media = 0;
    temp.valor_max = 0;
    temp.valor_min = -1;
    temp.count = 0;

    for (int i = 0; i < max_keys; i++)
    {
        sensores[i] = temp;
    }
    // Cria uma variavel temporaria do tipo alerta com os parametros todos a NULL ou 0 para inicializar todos os alertas com esses parametros
    alerta temp_al;

    temp_al.id[0] = '\0';
    temp_al.chave[0] = '\0';
    temp_al.valor_max = 0;
    temp_al.valor_min = 0;
    temp_al.consola = -1;

    for (int i = 0; i < max_alerts; i++)
    {
        alertas[i] = temp_al;
    }

    //----------------------------------------------

    // --------------Criar semáforo---------------------
    sem_unlink("SEM_SYS");
    sem_sys = sem_open("SEM_SYS", O_CREAT | O_EXCL, 0700, 1);
    if (sem_sys == SEM_FAILED)
    {
        write_log("ERRO AO ABRIR SEMAFORO\n");
                kill(pai, SIGINT);

        exit(1);
    }
    else
        write_log("SEMAFORO CRIADO COM SUCESSO!\n");
    //----------------------------------------------

    //--------------INTERNAL QUEUE------------------
    Mensagem lista[queue_sz];
    inter_queue = lista;
    for (int i = 0; i < queue_sz; i++)
    {
        inter_queue[i].cmd[0] = '\0';
    }
    //-----------------------------------------------

    //--------------VARIAVEL DE CONDIÇÃO E MUTEX------------------

    // Inicializa o atributo da variável de condição
    pthread_condattr_init(&attrcond);
    pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED);

    /* Initialise attribute to mutex. */
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

    // Aloca memória para a variável de condição
    pcond = (pthread_cond_t *)((char *)n_chaves + sizeof(int));

    // Allocate memory to pmutex here
    pmutex = (pthread_mutex_t *)((char *)pcond + sizeof(pthread_cond_t));

    /* Initialise mutex. */
    if (pthread_mutex_init(pmutex, &attrmutex) != 0)
    {
        perror("Erro ao inicializar a variável de condição");
        exit(1);
    }

    /* Initialise condition. */
    if (pthread_cond_init(pcond, &attrcond) != 0)
    {
        perror("Erro ao inicializar a variável de condição");
        exit(1);
    }
    //----------------------------------------------------------
}

void read_config_file(char *file)
{
    FILE *fp;
    char line[100];
    int config[5];
    int i = 0;

    fp = fopen(file, "r");
    if (fp == NULL)
    {
        write_log("Erro ao abrir o arquivo de configuração.\n");
        exit(0);
    }

    while (fgets(line, 100, fp) != NULL)
    {
        config[i] = atoi(line);
        i++;
    }

    fclose(fp);

    queue_sz = config[0];
    n_workers = config[1];
    max_keys = config[2];
    max_sensors = config[3];
    max_alerts = config[4];

    // realiza a validação das configurações lidas
    if (queue_sz < 1 || n_workers < 1 || max_keys < 1 || max_sensors < 1 || max_alerts < 0)
    {
        write_log("Erro: valores inválidos lidos do arquivo de configurações.\n");
        exit(1);
    }
}

void criar_threads()
{
    pthread_create(&console_thread, NULL, console_reader, NULL);
    pthread_create(&sensor_thread, NULL, sensor_reader, NULL);
    pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
}

void criar_workers()
{
    pipes_p = malloc(n_workers * sizeof(*pipes_p));
    if (pipes_p == NULL)
    {
        sem_wait(sem_sys);
        write_log("Erro ao alocar memoria para pipes.\n");
        sem_post(sem_sys);
        kill(pai, SIGINT);
    }

    char log[256];
    // Criação dos Workers
    for (int i = 0; i < n_workers; i++)
    {
        if (pipe(pipes_p[i]) < 0)
        {
            sem_wait(sem_sys);
            write_log("Erro ao criar pipe.\n");
            sem_post(sem_sys);
            kill(pai, SIGINT);

        }

        pid_t worker = fork();

        if (worker == 0)
        {
            sprintf(log, "WORKER %d READY", i);

            sem_wait(sem_sys);
            write_log(log);
            sem_post(sem_sys);

            worker_func(i);
        }
        else if (worker < 0)
        {
            sem_wait(sem_sys);
            write_log("Erro ao criar processo Worker.\n");
            sem_post(sem_sys);

            exit(1);
        }

        workers[i] = malloc(sizeof(pid_t));
        *(workers[i]) = worker;
    }
}

void criar_alert_watcher()
{
    watcher = fork();
    if (watcher == 0)
    {
        sem_wait(sem_sys);
        write_log("PROCESS ALERTS_WATCHER CREATED\n");
        sem_post(sem_sys);

        alerts_func();
    }
    else if (watcher < 0)
    {
        sem_wait(sem_sys);
        write_log("Erro ao criar processo Alert Wacher.\n");
        sem_post(sem_sys);

        exit(1);
    }
}

void *console_reader()
{
    int fd;
    Mensagem msg;
    fd_set read_fds;

    sem_wait(sem_sys);
    write_log("THREAD CONSOLE_READER CREATED\n");
    sem_post(sem_sys);

    if ((fd = open(CONSOLE_PIPE, O_RDONLY)) == -1)
    {
        perror("Erro ao abrir pipe");
        return NULL;
    }

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    while (1)
    {
        // Aguardando até que haja dados prontos para serem lidos no pipe
        if (select(fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Erro na chamada select()");
            continue;
        }

        // Verificar se o pipe tem algo para ler
        if (FD_ISSET(fd, &read_fds))
        {
            // Ler dados do pipe
            memset(msg.cmd, 0, sizeof(msg.cmd));
            ssize_t bytes_lidos = read(fd, &msg.cmd, sizeof(msg.cmd));
            if (bytes_lidos == -1)
            {
                perror("Erro ao ler no pipe");
                continue;
            }
            else if (bytes_lidos == 0)
            {
                continue;
            }

            // printf("\nComando recebido do pipe: %s\n", msg.cmd);
            msg.tipo = 2;

            sem_wait(sem_sys);
            // Verifica espaço vazio na lista e envia a mensagem
            for (int i = 0; i < queue_sz; i++)
            {
                if (i == (queue_sz - 1))
                {
                    write_log("FILA INTERNA CHEIA, COMANDO DESCARTADO\n");
                    break;
                }
                else if (inter_queue[i].cmd[0] == '\0')
                {
                    inter_queue[i] = msg;
                    num_mensagens++;
                    break;
                }
            }
            sem_post(sem_sys);
        }
    }

    close(fd);
    return NULL;
}

void *sensor_reader()
{
    Mensagem msg;
    int fd;

    sem_wait(sem_sys);
    write_log("THREAD SENSOR_READER CREATED\n");
    sem_post(sem_sys);

    if ((fd = open(SENSOR_PIPE, O_RDONLY)) == -1)
    {
        perror("Erro ao abrir pipe");
        return NULL;
    }

    // Descritores de ficheiros
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    while (1)
    {
        // Aguardando até que haja dados prontos para serem lidos no pipe
        if (select(fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Erro na chamada select()");
            continue;
        }

        // Verificando se o pipe tem algo para ler
        if (FD_ISSET(fd, &read_fds))
        {
            memset(msg.cmd, 0, sizeof(msg.cmd)); // Limpa a estrutura antes de ler
            // ler dados do pipe
            ssize_t bytes_lidos = read(fd, &msg.cmd, sizeof(msg.cmd));
            if (bytes_lidos == -1)
            {
                perror("Erro ao ler no pipe");
                continue;
            }
            else if (bytes_lidos == 0)
            {
                continue;
            }

            // processar mensagem recebida
            // printf("Mensagem recebida de sensor: %s\n", msg.cmd);
            msg.tipo = 1;

            sem_wait(sem_sys);
            // Verifica espaço vazio na lista e coloca a mensagem
            for (int i = 0; i < queue_sz; i++)
            {
                if (i == (queue_sz - 1))
                {
                    write_log("FILA INTERNA CHEIA, COMANDO DESCARTADO\n");
                    break;
                }
                else if (inter_queue[i].cmd[0] == '\0')
                {
                    inter_queue[i] = msg;
                    num_mensagens++;
                    break;
                }
            }
            sem_post(sem_sys);
        }
    }

    close(fd);

    return NULL;
}

void *dispatcher()
{
    sem_wait(sem_sys);
    write_log("THREAD DISPATCHER CREATED\n");
    sem_post(sem_sys);

    Mensagem msg;
    char buffer[500];

    while (1)
    {
        int controlo = 0;
        // printf("A tentar ler mensagens 2\n");

        sem_wait(sem_sys);
        // Tentar ler mensagem com prioridade 2
        for (int i = 0; i < num_mensagens; i++)
        {
            if ((inter_queue[i].cmd[0] != '\0') && (inter_queue[i].tipo == 2))
            {
                msg = inter_queue[i];
                controlo = 1;

                // Remover mensagem encontrada da lista e fazer o "shift" das mensagens restantes
                for (int j = i; j < num_mensagens - 1; j++)
                {
                    inter_queue[j] = inter_queue[j + 1];
                }
                inter_queue[num_mensagens - 1].cmd[0] = '\0'; // Limpar o último espaço

                num_mensagens--;
                break;
            }
        }

        // Não há mensagens com prioridade 2 na fila, tentar ler mensagem com prioridade 1
        if (controlo == 0)
        {
            // printf("A tentar ler mensagens 1\n");

            for (int i = 0; i < num_mensagens; i++)
            {
                if ((inter_queue[i].cmd[0] != '\0') && (inter_queue[i].tipo == 1))
                {
                    msg = inter_queue[i];
                    controlo = 1;

                    // Remover mensagem encontrada da lista e fazer o "shift" das mensagens restantes
                    for (int j = i; j < num_mensagens - 1; j++)
                    {
                        inter_queue[j] = inter_queue[j + 1];
                    }
                    inter_queue[num_mensagens - 1].cmd[0] = '\0'; // Limpar o último espaço
                    num_mensagens--;
                    break;
                }
            }
        }
        sem_post(sem_sys);

        if (controlo == 0)
        {
            continue;
        }

        // printf("Comando recebido da fila interna %s\n", msg.cmd);

        sprintf(buffer, "%s*%ld", msg.cmd, msg.tipo);

        for (int i = 0; i < n_workers; i++)
        {
            // Verifica se o processo está bloqueado (disponivel)
            sem_wait(sem_sys);
            if (worker_status[i] == 0)
            {
                // escreve no pipe
                if (write(pipes_p[i][1], buffer, strlen(buffer) + 1) < 0)
                {
                    perror("Erro ao escrever no pipe");
                    kill(getpid(), SIGINT);
                }
                sem_post(sem_sys);
                break;
            }
            else
                sem_post(sem_sys);
        }
    }

    return NULL;
}

void worker_func(int a)
{
    char buffer[255], buffer2[255];
    Mensagem msg;

    while (1)
    {
        // fecha o descritor de escrita do pipe
        close(pipes_p[a][1]);

        // atualiza o status do worker
        sem_wait(sem_sys);
        worker_status[a] = 0;
        sem_post(sem_sys);

        // lê do pipe - fica bloqueado até receber
        read(pipes_p[a][0], buffer, sizeof(buffer));

        // atualiza o status do worker
        sem_wait(sem_sys);
        worker_status[a] = 1;
        sem_post(sem_sys);

        strcpy(buffer2, buffer);
        char *tipo = strrchr(buffer, '*') + 1; // +1 para apontar para o caractere após o '*'
        char *diretiva = strtok(buffer, "$");

        if (strcmp(tipo, "1") == 0)
        {
            sensor_func(diretiva);

            sem_wait(sem_sys);

            // Sinalizar o alerts watcher
            pthread_mutex_lock(pmutex);
            pthread_cond_signal(pcond);
            pthread_mutex_unlock(pmutex);
            
            sem_post(sem_sys);
        }
        else if (strcmp(tipo, "2") == 0)
        {

            msg.tipo = atoi(strrchr(buffer2, '$') + 1);

            strcpy(msg.cmd, executar_comandos(diretiva, msg.tipo));

            // envia a mensagem para a consola
            if ((msgsnd(mqid, &msg, sizeof(Mensagem) - sizeof(long), 0)) == -1)
            {
                sem_wait(sem_sys);
                write_log("Erro ao enviar mensagem para a consola\n");
                sem_post(sem_sys);
            }
        }

        // libera a variável buffer para a próxima mensagem
        memset(buffer, 0, sizeof(buffer));
    }
}

char *executar_comandos(char *cmd, int consola)
{
    char log[256], mensagem[700], *mensagem2;
    char id_[32], chave_[32], dados[5][32];
    int min, max, i = 0;
    Sensor_data temp;
    char *token = strtok(cmd, " ");
    token = strtok(NULL, " ");

    // Limpar o conteudo da var mensagem
    memset(mensagem, 0, sizeof(mensagem));

    if (strcmp(cmd, "Stats") == 0)
    {
        sem_wait(sem_sys);
        if (*n_chaves == 0)
        {

            write_log("NAO EXISTEM SENSORES REGISTADOS\n");
            sem_post(sem_sys);
            strcpy(mensagem, "NAO EXISTEM SENSORES REGISTADOS\n");
        }
        else
        {
            write_log("Key Last Min Max Avg Count\n");
            strcpy(mensagem, "Key Last Min Max Avg Count\n");
            sem_post(sem_sys);

            while (1)
            {
                sem_wait(sem_sys);
                temp = sensores[i];

                if (temp.chave[0] != '\0')
                {
                    sprintf(log, "\n%s %d %d %d %d %d\n", temp.chave, temp.leitura, temp.valor_min, temp.valor_max, temp.media, temp.count);
                    write_log(log);
                    sem_post(sem_sys);
                    strcat(mensagem, log);
                    i++;
                }
                else // Quando deixa de haver sensores
                {
                    sem_post(sem_sys);
                    break;
                }
            }
        }
    }

    else if (strcmp(cmd, "Reset") == 0)
    {
        sem_wait(sem_sys);
        if (*n_chaves == 0)
        {
            write_log("NAO EXISTEM SENSORES REGISTADOS\n");
            sem_post(sem_sys);
            strcpy(mensagem, "NAO EXISTEM SENSORES REGISTADOS\n");
        }

        else
        {
            sem_post(sem_sys);

            while (1)
            {
                sem_wait(sem_sys);
                if (sensores[i].chave[0] != '\0')
                {
                    sensores[i].leitura = 0;
                    sensores[i].count = 0;
                    sensores[i].media = 0;
                    sensores[i].valor_max = 0;
                    sensores[i].valor_min = -1;
                    sensores[i].count = 0;

                    sem_post(sem_sys);
                    i++;
                    continue;
                }
                else
                {
                    sem_post(sem_sys);
                    strcpy(mensagem, "OK\n");
                    break;
                }
            }
        }
    }

    else if (strcmp(cmd, "Sensors") == 0)
    {
        sem_wait(sem_sys);

        if (*n_chaves == 0)
        {

            write_log("NAO EXISTEM SENSORES REGISTADOS\n");
            sem_post(sem_sys);
            strcpy(mensagem, "NAO EXISTEM SENSORES REGISTADOS\n");
        }
        else
        {
            while (sensores[i].chave[0] != '\0')
            {
                sprintf(log, "\n%s\n", sensores[i].id);
                write_log(log);

                strcat(mensagem, log);

                memset(log, 0, sizeof(log));
                i++;
            }
            sem_post(sem_sys);
        }
    }

    else if (strcmp(cmd, "Add_alert") == 0)
    {
        int controlo = 0;
        for (int i = 0; cmd != NULL; i++)
        {
            strcpy(dados[i], cmd);
            cmd = strtok(NULL, " ");
        }
        strcpy(id_, token);
        strcpy(chave_, dados[1]);
        min = atoi(dados[2]);
        max = atoi(dados[3]);

        for (int i = 0; i < max_alerts; i++)
        {
            if (strcmp(alertas[i].id, id_) == 0)
            {
                controlo = 1;
            }
        }

        if (controlo == 0)
        {
            for (int i = 0; i < max_alerts; i++)
            {

                sem_wait(sem_sys);
                if (alertas[i].chave[0] == '\0')
                {
                    strcpy(alertas[i].chave, chave_);
                    strcpy(alertas[i].id, id_);
                    alertas[i].valor_max = max;
                    alertas[i].valor_min = min;
                    alertas[i].consola = consola;

                    sprintf(log, "Criado alerta com id-%s, chave-%s, max-%d, min-%d", alertas[i].id, alertas[i].chave, alertas[i].valor_max, alertas[i].valor_min);
                    write_log(log);

                    sem_post(sem_sys);
                    strcpy(mensagem, "OK\n");
                    break;
                }
                else
                {
                    sem_post(sem_sys);
                    controlo++;
                }
            }
            sem_wait(sem_sys);
            if (controlo == max_alerts)
            {
                write_log("NÚMERO MÁXIMO DE ALERTAS ANTINGIDO\n");
                sem_post(sem_sys);
                strcpy(mensagem, "NÚMERO MÁXIMO DE ALERTAS ANTINGIDO\n");
            }
            else
            {
                sem_post(sem_sys);
            }
        }
        else
        {
            write_log("ERRO - ID ALERT JA EXISTENTE\n");
            sem_post(sem_sys);
            strcpy(mensagem, "ERRO - ID ALERT JA EXISTENTE\n");
        }
    }

    else if (strcmp(cmd, "Remove_alert") == 0)
    {
        int ctrl = 0;
        sem_wait(sem_sys);

        for (int i = 0; i < max_alerts; i++)
        {
            if (strcmp(alertas[i].id, token) == 0)
            {
                alertas[i].chave[0] = '\0';
                alertas[i].id[0] = '\0';
                alertas[i].valor_max = -1;
                alertas[i].valor_min = -1;

                sprintf(log, "Eliminado alerta com id-%s, chave-%s, max-%d, min-%d", alertas[i].id, alertas[i].chave, alertas[i].valor_max, alertas[i].valor_min);
                write_log(log);
                memset(log, 0, sizeof(log));
                strcpy(mensagem, "OK\n");
                ctrl = 1;
                break;
            }
        }
        sem_post(sem_sys);

        if (ctrl == 0)
        {
            sem_wait(sem_sys);
            write_log("ID NÃO ENCONTRADO\n");
            sem_post(sem_sys);
            strcpy(mensagem, "ID NÃO ENCONTRADO\n");
        }
    }

    else if (strcmp(cmd, "List_alerts") == 0)
    {
        sem_wait(sem_sys);
        write_log("ID Key MIN MAX\n");
        strcpy(mensagem, "ID Key MIN MAX\n");
        for (int i = 0; i < max_alerts; i++)
        {

            if (alertas[i].chave[0] != '\0')
            {

                sprintf(log, "\n%s, %s, %d, %d\n", alertas[i].id, alertas[i].chave, alertas[i].valor_min, alertas[i].valor_max);
                write_log(log);
                strcat(mensagem, log);
            }
        }
        sem_post(sem_sys);
    }
    mensagem2 = mensagem;
    memset(log, 0, sizeof(log));
    return mensagem2;
}

void sensor_func(char *dados)
{
    Sensor_data sensor, temp;

    strcpy(sensor.id, strtok(dados, "#"));
    strcpy(sensor.chave, strtok(NULL, "#"));
    sensor.leitura = atoi(strtok(NULL, "#"));

    for (int i = 0; i < max_keys; i++)
    {
        sem_wait(sem_sys);

        temp = sensores[i];

        sem_post(sem_sys);

        if (temp.id[0] != '\0' && strcmp(temp.chave, sensor.chave) == 0)
        {
            temp.leitura = sensor.leitura;
            temp.media = (temp.media * temp.count + temp.leitura) / (temp.count + 1);
            if (sensor.leitura > temp.valor_max)
                temp.valor_max = sensor.leitura;
            if (sensor.leitura < temp.valor_min || temp.valor_min < 0)
                temp.valor_min = sensor.leitura;
            temp.count++;

            sem_wait(sem_sys);
            sensores[i] = temp;
            sem_post(sem_sys);

            break;
        }
        else if (temp.id[0] != '\0' && strcmp(temp.chave, sensor.chave) != 0)
        {
            continue;
        }
        else if (temp.id[0] == '\0')
        {
            sem_wait(sem_sys);
            (*n_chaves)++;
            int chave = *n_chaves;
            sem_post(sem_sys);

            if (chave <= max_keys)
            {
                strcpy(temp.chave, sensor.chave);
                strcpy(temp.id, sensor.id);
                temp.leitura = sensor.leitura;
                temp.valor_max = sensor.leitura;
                temp.valor_min = sensor.leitura;
                temp.media = sensor.leitura;
                temp.count = 1;

                sem_wait(sem_sys);
                sensores[i] = temp;
                sem_post(sem_sys);

                break;
            }
            else
            {
                sem_wait(sem_sys);
                write_log("Atingido o número máximo de chaves!\n");
                sem_post(sem_sys);
                break;
            }
        }
    }
}

void alerts_func()
{
    Mensagem msg;
    pthread_mutex_lock(pmutex);

    while (1)
    {
        pthread_cond_wait(pcond, pmutex);

        sem_wait(sem_sys);
        for (int i = 0; i < max_alerts; i++)
        {
            if (alertas[i].chave[0] != '\0')
            {
                for (int j = 0; j < max_sensors; j++)
                {
                    if (strcmp(alertas[i].chave, sensores[j].chave) == 0)
                    {

                        if (sensores[j].leitura > alertas[i].valor_max || sensores[j].leitura < alertas[i].valor_min)
                        {
                            sprintf(msg.cmd, "ALERTA %s DISPAROU COM A LEITURA %d\n", alertas[i].chave, sensores[j].leitura);
                            msg.tipo = alertas[i].consola;
                            msgsnd(mqid, &msg, sizeof(Mensagem) - sizeof(long), 0);
                        }
                        break;
                    }
                }
            }
        }
        sem_post(sem_sys);
    }
    pthread_mutex_unlock(pmutex);
}

void write_log(char *info)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    // escreve a informação na consola
    printf("%s\n", info);

    // escreve a informação no arquivo de texto
    fprintf(log_file, "[%s] %s\n", timestamp, info);
}

void cleanup()
{
    sem_wait(sem_sys);
    write_log("\nHOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH\n");
    sem_post(sem_sys);

    // Mata os processos filhos
    for (int i = 0; i < n_workers; i++)
    {
        kill(*workers[i], SIGTERM);
    }

    // Mata o Alerts Watcher
    kill(watcher, SIGTERM);

    // Espera pelo fim dos processos
    while (wait(NULL) > 0)
        ;

    write_log("HOME_IOT SIMULATOR CLOSING\n");

    // Limpa a memória alocada para o array de Workers
    for (int i = 0; i < n_workers; i++)
    {
        free(workers[i]);
    }

    free(workers);
    free(pipes_p);

    // Cancela e aguarda termino de threads
    pthread_cancel(console_thread);
    pthread_cancel(sensor_thread);
    pthread_cancel(dispatcher_thread);
    pthread_join(console_thread, NULL);
    pthread_join(sensor_thread, NULL);
    pthread_join(dispatcher_thread, NULL);

    // Limpa o mutex
    pthread_mutex_destroy(pmutex);
    pthread_mutexattr_destroy(&attrmutex);

    /*// Limpa a variável de condição
    pthread_cond_destroy(pcond);
    pthread_condattr_destroy(&attrcond);*/

    // Limpa a shared memory
    if (shmdt(sensores) == -1)
        write_log("ERRO A REMOVER SENSORES\n");

    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        write_log("ERRO A REMOVER SHM\n");

    // Eliminar Pipes
    unlink(SENSOR_PIPE);
    unlink(CONSOLE_PIPE);

    // Fechar Message Queue
    if ((msgctl(mqid, IPC_RMID, NULL)) == -1)
    {
        perror("Erro ao remover fila de mensagens");
    }

    // Fechar Semaforo
    sem_close(sem_sys);
    sem_unlink("SEM_SYS");

    // Fechar Log File
    fclose(log_file);

    exit(0);
}
