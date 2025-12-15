#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

pthread_mutex_t fileMutex;

void* clientThread(void* arg) {
    int sock = ((int)arg);
    free(arg);

    pthread_detach(pthread_self());

    char buffer[1024];
    FILE* f;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int r = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (r <= 0) {
            printf("[Client disconnected]\n");
            break;
        }

        buffer[r] = 0;
        printf("[Received] %s", buffer);

        // --- GHI XUỐNG FILE CÓ LOCK MUTEX ---
        pthread_mutex_lock(&fileMutex);

        f = fopen("tmp.txt", "a");
        if (f != NULL) {
            fprintf(f, "%s\n", buffer);
            fclose(f);
        }

        pthread_mutex_unlock(&fileMutex);
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_mutex_init(&fileMutex, NULL);

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(serverSock, 10);
    printf("=== Telnet Multi-Client Server Started (C version) ===\n");

    while (1) {
        int clientSock = accept(serverSock, NULL, NULL);
        if (clientSock < 0) {
            perror("accept");
            continue;
        }

        printf("[New client connected]\n");

        int* arg = malloc(sizeof(int));
        *arg = clientSock;

        pthread_t tid;
        pthread_create(&tid, NULL, clientThread, arg);
    }

    close(serverSock);
    pthread_mutex_destroy(&fileMutex);
    return 0;
}