#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>

#define BUFFER 1024

// Глобальный флаг завершения
atomic_bool is_server_running = ATOMIC_VAR_INIT(true);
atomic_int clients = ATOMIC_VAR_INIT(0);

const int max_customer = 20;
const int max_observer = 10;
int observer_sockets[max_observer];
int num_observers = 0;
int observer_socket; // Socket descriptor for observer client
pthread_mutex_t observer_mutex;
pthread_t tid[2];
int count_clients = 0;

// Состояние покупателя
typedef enum {
    FIRST = 1, // к 1 кассиру
    SECOND = 2 // ко 2 кассиру
} CustomerState;

typedef struct {
    CustomerState state;
    int id;
    int is_served;
} Customer;

typedef struct {
    Customer customer[max_customer];
    int num_customer;
    bool is_start;
    int num_customer_cashier_1;
    int num_customer_cashier_2;
    int num_customer_cashier_1_served;
    int num_customer_cashier_2_served;
    int customer_cashier_1[max_customer];
    int customer_cashier_2[max_customer];
} Store;

Store *store;

void sigfunc(int sig) {
    if (sig != SIGINT && sig != SIGTERM) {
        return;
    }
    atomic_store(&is_server_running, false);
    printf("Server is shutting down...\n");
    sleep(5);
    printf("Sig finished\n");
    for (int i = 0; i < num_observers; i++) {
        close(observer_sockets[i]);
    }
    exit(10);

}

// Function to send console output to the observer client
void sendToObserver(const char *message) {
    pthread_mutex_lock(&observer_mutex);
    for (int i = 0; i < num_observers; i++) {
        send(observer_sockets[i], message, strlen(message), 0);
    }
    pthread_mutex_unlock(&observer_mutex);

}

// Thread function to handle the observer client connection
void *observerHandler(void *socket_desc) {
    observer_socket = *(int *) socket_desc;
    pthread_mutex_lock(&observer_mutex);
    if (num_observers < max_observer) {
        observer_sockets[num_observers] = observer_socket;
        num_observers++;
        printf("Observer client connected. Total observers: %d\n", num_observers);
        send(observer_socket, "Connected to server.\n", strlen("Connected to server.\n"), 0);
    } else {
        printf("Maximum number of observers reached. Rejecting new connection.\n");
        send(observer_socket, "Server reached maximum capacity. Cannot accept new observers.\n",
             strlen("Server reached maximum capacity. Cannot accept new observers.\n"), 0);
        close(observer_socket);
        free(socket_desc);
        pthread_mutex_unlock(&observer_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&observer_mutex);

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
    pthread_mutex_lock(&observer_mutex);
    for (int i = 0; i < num_observers; i++) {
        if (observer_sockets[i] == observer_socket) {
            // Удаляем сокет клиента-наблюдателя из массива
            for (int j = i; j < num_observers - 1; j++) {
                observer_sockets[j] = observer_sockets[j + 1];
            }
            num_observers--;
            printf("Observer client disconnected. Total observers: %d\n", num_observers);
            break;
        }
    }
    pthread_mutex_unlock(&observer_mutex);

    // Close observer client socket
    close(observer_socket);
    free(socket_desc);

    printf("Observer client disconnected.\n");
}

 //Функция потока для обработки клиентского подключения
void *cashierHandler(void *socket_desc) {
    int new_socket = *(int *) socket_desc;
    int cashier_id;
    int is_ready = 1;
    int answer;
    if (!atomic_load(&is_server_running)) {
        // Флаг завершения установлен, завершаем работу
        // Закрываем сокет для клиента
        close(new_socket);
        // Освобождаем память выделенную для сокета
        free(socket_desc);
        // Завершаем поток
        pthread_exit(NULL);
    }
    // Получаем значения id от клиента
    ssize_t bytesSent  = recv(new_socket, &cashier_id, sizeof(int), 0);
    // обработка ошибки
    if (bytesSent <= 0) {
        close(new_socket);
        free(socket_desc);
        pthread_exit(NULL);
    }
    char buffer2[BUFFER];
    sprintf(buffer2,"Cashier has id%d.\n", cashier_id);
    sendToObserver(buffer2);
    printf("Cashier has id%d.\n", cashier_id);
    if (cashier_id == 1) {
        // Отправляем клиенту количество покупателей в очереди
        // количество покупателей в очереди к 1 кассиру
        int num_customer_cashier_1 = store->num_customer_cashier_1 - store->num_customer_cashier_1_served;
        ssize_t bytes_sent = send(new_socket, &num_customer_cashier_1, sizeof(int), 0);
        if (bytes_sent <= 0) {
            close(new_socket);
            free(socket_desc);
            pthread_exit(NULL);
        }
            if (!atomic_load(&is_server_running)) {
                close(new_socket);
                free(socket_desc);
                pthread_exit(NULL);
            }
            sleep(1);
            for (int i = 0; i < store->num_customer_cashier_1; i++) {
                if (!atomic_load(&is_server_running)) {
                    close(new_socket);
                    free(socket_desc);
                    pthread_exit(NULL);
                }
                if (store->customer[store->customer_cashier_1[i]].state == FIRST && cashier_id == 1 &&
                    store->customer[store->customer_cashier_1[i]].is_served == 0) {
                        ssize_t bytesSent = recv(new_socket, &is_ready, sizeof(int), 0);
                        if (bytesSent <= 0) {
                            close(new_socket);
                            free(socket_desc);
                            pthread_exit(NULL);
                        }
                        answer = 0;
                        char buffer1[BUFFER];
                        sprintf(buffer1, "Cashier №%d is ready for work.\n", cashier_id);
                        sendToObserver(buffer1);
                        printf("Cashier №%d is ready for work.\n", cashier_id);
                            // отправляем клиенту id покупателя;
                            answer = store->customer[store->customer_cashier_1[i]].id;
                            ssize_t bytes_sent = send(new_socket, &answer, sizeof(int), 0);
                            if (bytes_sent <= 0) {
                                close(new_socket);
                                free(socket_desc);
                                pthread_exit(NULL);
                            }
                            char buffer[BUFFER];
                            sprintf(buffer, "Cashier №%d is served customer №%d\n", cashier_id, answer);
                            sendToObserver(buffer);
                            printf("Cashier №%d is served customer №%d\n", cashier_id, answer);
                            store->customer[store->customer_cashier_1[i]].is_served = 1;
                            store->num_customer_cashier_1_served++;
                            sleep(1);
                            if (store->num_customer_cashier_1_served == store->num_customer_cashier_1) {
                                break;
                            }
                }

                if (!atomic_load(&is_server_running)) {
                    answer = -1;
                    close(new_socket);
                    free(socket_desc);
                    pthread_exit(NULL);
                }
            }
            sleep(1);
            char buffer[BUFFER];
            sprintf(buffer,"Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    store->num_customer_cashier_1_served);
            sendToObserver(buffer);
            printf(
                    "Cashier №%d is going home. He served: %d customer\n",
                    cashier_id,
                    store->num_customer_cashier_1_served
            );
    } else {
        // Отправляем клиенту количество покупателей в очереди
        // количество покупателей в очереди к 2 кассиру
        int num_customer_cashier_2 = store->num_customer_cashier_2 - store->num_customer_cashier_2_served;
        ssize_t bytes_sent =  send(new_socket, &num_customer_cashier_2, sizeof(int), 0);
        if (bytes_sent == -1) {
            close(new_socket);
            free(socket_desc);
            pthread_exit(NULL);
        }
            if (!atomic_load(&is_server_running)) {
                close(new_socket);
                free(socket_desc);
                pthread_exit(NULL);
            }
            sleep(1);
            char buffer1[BUFFER];
            sprintf(buffer1,"Cashier №%d is ready for work.\n", cashier_id);
            sendToObserver(buffer1);
            printf("Cashier №%d is ready for work.\n", cashier_id);
            is_ready = 1;
            for (int i = 0; i < store->num_customer_cashier_2; i++) {
                if (!atomic_load(&is_server_running)) {
                    close(new_socket);
                    free(socket_desc);
                    pthread_exit(NULL);
                }
                if (store->customer[store->customer_cashier_2[i]].state == SECOND && cashier_id == 2 &&
                    store->customer[store->customer_cashier_2[i]].is_served == 0) {
                    ssize_t bytesSent = recv(new_socket, &is_ready, sizeof(int), 0);
                    // обработка ошибки
                    if (bytesSent <= 0) {
                        close(new_socket);
                        free(socket_desc);
                        pthread_exit(NULL);
                    }

                    answer = 0;
                    // отправляем клиенту id покупателя;
                    answer = store->customer[store->customer_cashier_2[i]].id;
                    ssize_t bytes_sent = send(new_socket, &answer, sizeof(int), 0);
                    if (bytes_sent == -1) {
                        close(new_socket);
                        free(socket_desc);
                        pthread_exit(NULL);
                    }
                    char buffer[BUFFER];
                    sprintf(buffer, "Cashier №%d is served customer №%d\n", cashier_id, answer);
                    sendToObserver(buffer);
                    printf("Cashier №%d is served customer №%d\n", cashier_id, answer);
                    store->customer[store->customer_cashier_2[i]].is_served = 1;
                    store->num_customer_cashier_2_served++;
                    sleep(1);
                    if (store->num_customer_cashier_2_served == store->num_customer_cashier_2) {
                        break;
                    }

                    if (!store->is_start) {
                        answer = -1;
                    }
                }
                sleep(1);
            }
                char buffer[BUFFER];
                sprintf(buffer, "Cashier №%d is going home. He served: %d customer\n",
                        cashier_id,
                        store->num_customer_cashier_2_served);
                sendToObserver(buffer);
                printf(
                        "Cashier №%d is going home. He served: %d customer\n",
                        cashier_id,
                        store->num_customer_cashier_2_served
                );
    }
    atomic_fetch_add(&clients, 1); // Увеличиваем число обслуженных клиентов
    if (clients == 2) {
        printf("All of cashier is connected.\n");
        for (int i = 0; i < count_clients; i++) {
            pthread_join(tid[i], NULL);
        }

        free(store);

        printf("All cashier threads have finished. Server is shutting down.\n");
        // закрываем сервер
        atomic_store(&is_server_running, 0);
        for (int i = 0; i < num_observers; i++) {
            close(observer_sockets[i]);
        }
        exit(10);
    }
    close(new_socket);
    free(socket_desc);
    pthread_exit(NULL);
}


void readCustomers(void *socket_desc) {
    int new_socket = *(int *) socket_desc;
    int num_customer;
    if (!atomic_load(&is_server_running)) {
        close(new_socket);
        free(socket_desc);
    }
    int ok = 1;
    read(new_socket, &num_customer, sizeof(int));
    store->num_customer = num_customer;
    store->num_customer_cashier_1 = 0;
    store->num_customer_cashier_2 = 0;
    char buffer[BUFFER];
    sprintf(buffer,"In store waiting %d customers.\n", num_customer);
    sendToObserver(buffer);
    printf("In store waiting %d customers.\n", num_customer);
    ssize_t bytes_sent = send(new_socket, &ok, sizeof(int), 0);
    if (bytes_sent == -1) {
        close(new_socket);
        free(socket_desc);
    }

    srand(time(NULL));
    for (int i = 0; i < num_customer; i++) {
        if (!atomic_load(&is_server_running)) {
            break;
        }
        // cчитываем индексы покупателей
        int id;
        ssize_t bytesSent  = read(new_socket, &id, sizeof(int));
        // обработка ошибки
        if (bytesSent == -1) {
        printf("Client is disconnected.\n");
            break;
        } else if (bytesSent != sizeof(int)) {
            printf("Client is disconnected.\n");
            break;
        }
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
            store->customer[i].is_served = 0;

        } else {
            store->customer_cashier_2[store->num_customer_cashier_2] = i;
            store->num_customer_cashier_2++;
            store->customer[i].state = SECOND;
            store->customer[i].is_served = 0;
        }
    }
    close(new_socket);
    free(socket_desc);
}

void handle_sigpipe(int signum) {

    printf("Client is disconnected.\n");
}
int main(int argc, char const *argv[]) {
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);
    signal(SIGPIPE, handle_sigpipe);
    unsigned short server_port;
    int server_fd, new_socket;
    pthread_mutex_init(&observer_mutex, NULL);
    struct sockaddr_in address;
    int addrlen = sizeof(address);

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
    if (listen(server_fd, 3 + max_observer) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
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
            // Accept the observer client connection

            // Create a thread to handle the observer client connection
            pthread_t observer_thread;
            if (pthread_create(&observer_thread, NULL, observerHandler, socket_desc) != 0) {
                perror("could not create observer thread");
                return -1;
            }
        } else {
            printf("Generator is connected.\n");
            readCustomers(socket_desc);
            printf("All of customer in queue\n"); // Добавлено: после вызова readCustomers
            break;
        }
    }
    store->is_start = true;

    // Обрабатываем каждое новое подключение в отдельном потоке
    while ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) && clients < 2 && atomic_load(&is_server_running)) {
        // Создаем новый сокет для клиента
        int *socket_desc = malloc(1);
        *socket_desc = new_socket;
        int id;
        read(new_socket, &id, sizeof(int));
        sleep(1);
        if (id == 2) {
            printf("Cashier is connected.\n");
            // Создаем поток для обработки клиентского подключения
            if (pthread_create(&tid[count_clients], NULL, cashierHandler, socket_desc) > 0) {
                perror("could not create thread");
                return -1;
            }
            count_clients++;

        } else {
            pthread_t observer_thread;
            if (pthread_create(&observer_thread, NULL, observerHandler, socket_desc) != 0) {
                perror("could not create observer thread");
                return -1;
            }
        }
    }
    return 0;
}