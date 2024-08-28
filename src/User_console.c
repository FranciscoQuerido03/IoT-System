#include "console.h"

int main(int argc, char *argv[])
{
    char id[32], chave[32], dados[5][32];
    int min, max;
    char *input;

    if (argc != 2)
    {
        printf("\nNúmero de argumentos incorreto!\nSintaxe: '$ User_console {identificador da consola}'\n\n");
        kill(pai, SIGINT);
    }
    else if (atoi(argv[1]) <= 0)
    {
        printf("\nIdentificador da Consola incorreto! Inteiro superior a 0\n\n");
        kill(pai, SIGINT);
    }
    else
    {
        num_consola = atoi(argv[1]);
        printf("Consola %d criada!\n", num_consola);
    }

    pai = getpid();

    signal(SIGINT, cleanup);
    init();

    while (1)
    {

        int controlo = 0;
        // Apresenta o prompt do menu e lê o comando do usuário, guardando na variavel 'cmd'
        char cmd[256];
        printf("\nEscolha uma opção:\n");
        printf("Stats\n");
        printf("Reset\n");
        printf("Sensors\n");
        printf("Add_alert [id] [chave] [min] [max]\n");
        printf("Remove_alert [id]\n");
        printf("List_alerts\n");
        printf("Exit\n");
        printf("> ");

        fgets(cmd, sizeof(cmd), stdin);
        strtok(cmd, "\n");

        input = strtok(cmd, " ");

        if (strcmp(input, "Stats") == 0)
        {
            strcpy(comando, input);
        }
        else if (strcmp(input, "Reset") == 0)
        {
            strcpy(comando, input);
        }
        else if (strcmp(input, "Sensors") == 0)
        {
            strcpy(comando, input);
        }
        else if (strcmp(input, "Add_alert") == 0)
        {
            input = strtok(NULL, " ");

            for (int i = 0; input != NULL; i++)
            {
                strcpy(dados[i], input);
                input = strtok(NULL, " ");
            }

            strcpy(id, dados[0]);
            strcpy(chave, dados[1]);
            min = atoi(dados[2]);
            max = atoi(dados[3]);

            // Verifica o tamanho da chave
            if (strlen(chave) < 3 || strlen(chave) > 32)
            {
                printf("\nTamanho da chave incorreto\n");
                continue;
            }

            // Verifica cada caractere da chave
            for (int i = 0; i < strlen(chave); i++)
            {
                if (!isalnum(chave[i]) && chave[i] != '_')
                {
                    printf("\nFormato inválido para a chave!\n");
                    controlo = 1;
                    break;
                }
            }
            if (controlo == 1)
            {
                continue;
            }

            if (strlen(id) < 3 || strlen(id) > 32)
            {

                printf("\nO id deve ter entre 3 e 32 caracteres.\n\n");
                continue;
            }

            if (max < min)
            {

                printf("\nValores inválidos Min-%d > Max-%d\n\n", min, max);
                continue;
            }

            sprintf(comando, "Add_alert %s %s %d %d", id, chave, min, max);
        }
        else if (strcmp(input, "Remove_alert") == 0)
        {
            input = strtok(NULL, " ");
            sprintf(comando, "Remove_alert %s", input);
        }
        else if (strcmp(input, "List_alerts") == 0)
        {
            strcpy(comando, input);
        }
        else if (strcmp(input, "Exit") == 0)
        {
            kill(pai, SIGINT);
            break;
        }
        else
        {
            printf("Invalid input\n");
            continue;
        }

        sprintf(comando + strlen(comando), "$%d", num_consola);

        sem_wait(sem_console);
        if (write(fd, &comando, sizeof(comando)) == -1)
        {
            printf("Erro a escrever no pipe! Comando descartado!\n");
            continue;
        }
        sem_post(sem_console);
        memset(comando, 0, sizeof(comando));
    }
}

void init()
{
    // Abre o named pipe para escrita
    if ((fd = open(PIPE_NAME, O_WRONLY)) == -1)
    {
        printf("\nERRO AO ABRIR PIPE!\n\n");
        exit(1);
    }

    //Cria o semáforo
    sem_console = sem_open("sem_console", O_CREAT | O_EXCL, 0700, 1);
    if (sem_console == SEM_FAILED)
    {
        perror("ERRO AO ABRIR MUTEX\n");
        kill(SIGINT, pai);
    }
    else
    {
        printf("MUTEX CRIADO COM SUCESSO!\n");
    }

    // Abre a Message Queue
    key_t key = 30;
    if ((mqid = msgget(key, IPC_CREAT | 0777)) == -1)
    {
        printf("ERRO A CRIAR MESSAGE QUEUE\n");
        kill(pai, SIGINT);
    }

    // Criar a thread
    if (pthread_create(&thread, NULL, readFromMessageQueue, (void *)&key) != 0)
    {
        perror("Erro ao criar a thread");
        kill(pai, SIGINT);
    }
}

void *readFromMessageQueue(void *arg)
{

    Mensagem msg;

    // Receber mensagens da Message Queue
    while (1)
    {
        // Receber a mensagem
        if (msgrcv(mqid, &msg, sizeof(Mensagem) - sizeof(long), num_consola, 0) == -1)
        {
            perror("Erro ao receber a mensagem da Message Queue");
            kill(pai, SIGINT);
        }

        // Processar a mensagem recebida
        printf("\n--**--\nMensagem recebida do Sys Manager: \n%s--**--\n", msg.cmd);
    }

    pthread_exit(NULL);
}

void cleanup()
{
    printf("\nCONSOLA %d A ENCERRAR\n", num_consola);

    // Fechar o descritor de arquivo
    close(fd);

    if (pthread_cancel(thread) == -1)
    {
        printf("Erro a cancelar thread\n");
        exit(1);
    }
    // Aguardar a finalização da thread
    if (pthread_join(thread, NULL) != 0)
    {
        perror("Erro ao aguardar a finalização da thread");
        exit(1);
    }

    // Fechar Semaforo
    if(sem_close(sem_console)==-1){
        printf("Erro a fechar semáforo\n");
        exit(1);
    }

    if(sem_unlink("sem_console")==-1){
        printf("Erro a fechar semáforo\n");
        exit(1);
    }

    printf("Consola encerrada com sucesso!\n");
       
    exit(0);
}