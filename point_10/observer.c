#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    unsigned short server_port;
    const char *server_ip;

    int sock = 0;
    struct sockaddr_in serv_addr;
    int server_answer_1 = 0;
    int server_answer_2 = 0;


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
    int identificator = 3;
    send(sock, &identificator, sizeof(int), 0);
    sleep(1);
    // Receive and print messages from the server console
    char buffer[1024];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  // Добавляем завершающий нулевой символ
        // Обрабатываем полученное сообщение
        printf("%s", buffer);
    }

    // Close the observer client socket
    close(sock);

    return 0;
}
