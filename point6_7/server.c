#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER 1024


const int max_customer = 20;
int observer_socket; // Socket descriptor for observer client

// Состояние покупателя
typedef enum {
    FIRST = 1, // к 1 кассиру
    SECOND = 2 // ко 2 кассиру
} CustomerState;

typedef struct {
    CustomerState state;
    int id;
} Customer;

typedef struct {
    Customer customer[max_customer];
    int num_customer;
    bool is_start;
    int num_customer_cashier_1;
    int num_customer_cashier_2;
    int customer_cashier_1[max_customer];
    int customer_cashier_2[max_customer];
} Store;

Store *store;

void sigfunc(int sig) {
    if (sig != SIGINT && sig != SIGTERM) {
        return;
    }

    printf("Sig finished\n");
    exit(10);
}

// Function to send console output to the observer client
void sendToObserver(const char *message) {
    send(observer_socket, message, strlen(message), 0);
}

// Thread function to handle the observer client connection
void *observerHandler(void *socket_desc) {
    observer_socket = *(int *) socket_desc;
    char buffer[1024];

    while (1) {
        ssize_t bytesRead = read(observer_socket, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            // Observer client disconnected
            break;
        }

        buffer[bytesRead] = '\0';
        printf("Received message from observer: %s\n", buffer);
    }

    // Close observer client socket
    close(observer_socket);
    free(socket_desc);

    // Terminate the server if the observer client disconnects
    printf("Observer client disconnected. Server is shutting down.\n");
    exit(0);
}

// Функция потока для обработки клиентского подключения
void *cashierHandler(void *socket_desc) {

    int new_socket = *(int *) socket_desc;
    int cashier_id;
    int is_ready;
    int answer;
    int countOfServedCustomer = 0;

    // Получаем значения id от клиента
    read(new_socket, &cashier_id, sizeof(int));
    char buffer2[BUFFER];
    sprintf(buffer2,"Cashier has id%d.\n", cashier_id);
    sendToObserver(buffer2);
    printf("Cashier has id%d.\n", cashier_id);
    if (cashier_id == 1) {
        send(new_socket, &store->num_customer_cashier_1, sizeof(int), 0);
        while (store->is_start) {
            sleep(1);
            char buffer1[BUFFER];
            sprintf(buffer1,"Cashier №%d is ready for work.\n", cashier_id);
            sendToObserver(buffer1);
            printf("Cashier №%d is ready for work.\n", cashier_id);
            for (int i = 0; i < store->num_customer_cashier_1; i++) {
                read(new_socket, &is_ready, sizeof(int));
                answer = 0;
                if (is_ready == 1) { // Добавленная проверка is_ready
                    if (store->customer[store->customer_cashier_1[i]].state == FIRST && cashier_id == 1) {
                        // отправляем клиенту id покупателя;
                        answer = store->customer[store->customer_cashier_1[i]].id;
                        countOfServedCustomer++;
                        char buffer[BUFFER];
                        sprintf(buffer,"Cashier №%d is served customer №%d\n", cashier_id, answer);
                        sendToObserver(buffer);
                        printf("Cashier №%d is served customer №%d\n", cashier_id, answer);
                        send(new_socket, &answer, sizeof(int), 0);
                        sleep(1);
                        if (countOfServedCustomer == store->num_customer_cashier_1) {
                            break;
                        }
                    }
                }

                if (!store->is_start) {
                    answer = -1;
                }
            }
            sleep(1);
            send(new_socket, &countOfServedCustomer, sizeof(int), 0);
            sleep(1);
            char buffer[BUFFER];
            sprintf(buffer,"Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    countOfServedCustomer);
            sendToObserver(buffer);
            printf(
                    "Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    countOfServedCustomer
            );

            // Закрываем сокет для клиента
            close(new_socket);

            // Освобождаем память выделенную для сокета
            free(socket_desc);
            break;
        }
    } else {
        send(new_socket, &store->num_customer_cashier_2, sizeof(int), 0);
        while (store->is_start) {
            sleep(1);
            char buffer1[BUFFER];
            sprintf(buffer1,"Cashier №%d is ready for work.\n", cashier_id);
            sendToObserver(buffer1);
            printf("Cashier №%d is ready for work.\n", cashier_id);
            for (int i = 0; i < store->num_customer_cashier_2; i++) {
                read(new_socket, &is_ready, sizeof(int));
                answer = 0;
                if (is_ready == 1) { // Добавленная проверка is_ready
                    if (store->customer[store->customer_cashier_2[i]].state == SECOND && cashier_id == 2) {
                        // отправляем клиенту id покупателя;
                        answer = store->customer[store->customer_cashier_2[i]].id;
                        countOfServedCustomer++;
                        char buffer[BUFFER];
                        sprintf(buffer,"Cashier №%d is served customer №%d\n", cashier_id, answer);
                        sendToObserver(buffer);
                        printf("Cashier №%d is served customer №%d\n", cashier_id, answer);
                        send(new_socket, &answer, sizeof(int), 0);
                        sleep(1);
                        if (countOfServedCustomer == store->num_customer_cashier_2) {
                            break;
                        }
                    }
                }

                if (!store->is_start) {
                    answer = -1;
                }
            }
            sleep(1);
            send(new_socket, &countOfServedCustomer, sizeof(int), 0);
            sleep(1);
            char buffer[BUFFER];
            sprintf(buffer,"Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    countOfServedCustomer);
            sendToObserver(buffer);
            printf(
                    "Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    countOfServedCustomer
            );

            // Закрываем сокет для клиента
            close(new_socket);

            // Освобождаем память выделенную для сокета
            free(socket_desc);
            break;
        }
    }
}


void readCustomers(void *socket_desc) {
    int new_socket = *(int *) socket_desc;
    int num_customer;
    int ok = 1;
    read(new_socket, &num_customer, sizeof(int));
    store->num_customer = num_customer;
    store->num_customer_cashier_1 = 0;
    store->num_customer_cashier_2 = 0;
    char buffer[BUFFER];
    sprintf(buffer,"In store waiting %d customers.\n", num_customer);
    sendToObserver(buffer);
    printf("In store waiting %d customers.\n", num_customer);
    send(new_socket, &ok, sizeof(int), 0);
    srand(time(NULL));
    for (int i = 0; i < num_customer; i++) {
        // cчитываем индексы покупателей
        int id;
        read(new_socket, &id, sizeof(int));
        store->customer[i].id = id;
        int cashier = rand() % 2 + 1;
        char buffer[BUFFER];
        sprintf(buffer,"Customer №%d is in store and in queue for cashier №%d\n", store->customer[i].id, cashier);
        sendToObserver(buffer);
        printf("Customer №%d is in store and in queue for cashier №%d\n", store->customer[i].id, cashier);
        if (cashier == 1) {
            store->customer_cashier_1[store->num_customer_cashier_1] = i;
            store->num_customer_cashier_1++;
            store->customer[i].state = FIRST;

        } else {
            store->customer_cashier_2[store->num_customer_cashier_2] = i;
            store->num_customer_cashier_2++;
            store->customer[i].state = SECOND;
        }
    }
    close(new_socket);
    free(socket_desc);
}

int main(int argc, char const *argv[]) {
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);
    unsigned short server_port;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    if (argc != 2) {     /* Test for correct number of arguments */
        fprintf(stderr, "Usage:  %s <Server Port>\n", argv[0]);
        exit(1);
    }

    server_port= atoi(argv[1]);  /* First arg:  local port */

    /* Create socket for incoming connections */
    if ((server_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    /* Construct local address structure */
    memset(&address, 0, sizeof(address));   /* Zero out structure */
    address.sin_family = AF_INET;                /* Internet address family */
    address.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    address.sin_port = htons(server_port);      /* Local port */


    // Привязываем сокет к адресу и порту
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Store server is start\n");



    // Слушаем входящие подключения
    if (listen(server_fd, 4) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in observer_client_address;
    socklen_t observer_client_address_len = sizeof(observer_client_address);
    observer_socket = accept(server_fd, (struct sockaddr *) &observer_client_address, &observer_client_address_len);
    if (observer_socket < 0) {
        perror("observer accept");
        exit(EXIT_FAILURE);
    }

    // Create a thread to handle the observer client connection
    pthread_t observer_thread;
    if (pthread_create(&observer_thread, NULL, observerHandler, &observer_socket) != 0) {
        perror("could not create observer thread");
        return -1;
    }

    store = malloc(sizeof(Store));
    // Принимаем подключение генератора покупателей
    while ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen))) {
        // Создаем новый сокет для клиента
        int *socket_desc = malloc(1);
        *socket_desc = new_socket;
        int id;
        read(new_socket, &id, sizeof(int));
        if (id != 1) {
            close(new_socket);
        } else {
            printf("Generator is connected.\n");
            readCustomers(socket_desc);
            printf("All of customer in queue\n"); // Добавлено: после вызова readCustomers
            break;
        }
    }
    store->is_start = true;
    int count_clients = 0;

    pthread_t tid[2];
    // Обрабатываем каждое новое подключение в отдельном потоке
    while ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen))) {
        // Создаем новый сокет для клиента
        int *socket_desc = malloc(1);
        *socket_desc = new_socket;
        int id;
        read(new_socket, &id, sizeof(int));
        sleep(1);
        printf("Cashier is connected.\n");
        // Создаем поток для обработки клиентского подключения
        if (pthread_create(&tid[count_clients], NULL, cashierHandler, socket_desc) > 0) {
            perror("could not create thread");
            return -1;
        }
        count_clients++;
        if (count_clients == 2) {
            break;
        }
    }
    for (int i = 0; i < count_clients; i++) {
        pthread_join(tid[i], NULL);
    }

    free(store);

    printf("All cashier threads have finished. Server is shutting down.\n");

    return 0;
}