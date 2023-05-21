#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>



void sigfunc(int sig) {
    if (sig != SIGINT && sig != SIGTERM) {
        return;
    }

    printf("Sig finished\n");
    exit(10);
}

int main(int argc, char const *argv[]) {
    unsigned short server_port;
    const char *server_ip;

    int sock = 0;
    struct sockaddr_in serv_addr;


    // Получаем значения port и server IP с помощью аргументов командной строки
    if (argc != 3) {
        printf("Args: <port> <SERVER_IP>\n");
        return -1;
    }

    server_port = atoi(argv[1]);
    server_ip = argv[2];

    // Создаем TCP сокет
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    // Устанавливаем адрес сервера
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Подключаемся к серверу
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    int identificator = 1;
    send(sock, &identificator, sizeof(int), 0);
    srand(time(NULL));
    int num_customers = rand() % 20 + 1;
    send(sock, &num_customers, sizeof(int), 0);

    int server_ok;
    read(sock, &server_ok, sizeof(num_customers));

    bool server_is_connected = true;

    while (server_is_connected) {

        for (int i = 0; i < num_customers; i++) {
            int id = i + 1;
            send(sock, &id, sizeof(int), 0);
            printf("Customer with id %d in the store.\n", id);
            sleep(1);
        }

        printf("All of customers in store.\n");
        server_is_connected = false;
    }
    return 0;
}