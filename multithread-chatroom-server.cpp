#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>  
#include <arpa/inet.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

int g_sockets[1024] = { 0 };
int g_count = 0;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int socket;
    int index;
} ThreadArg;

void* client_handler(void* arg) {
    ThreadArg* thread_arg = (ThreadArg*)arg;
    int client_socket = thread_arg->socket;
    int client_index = thread_arg->index;
    free(thread_arg);
    
    char buffer[1024];
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            printf("Client %d disconnected\n", client_socket);
            pthread_mutex_lock(&g_mutex);
            g_sockets[client_index] = 0;
            pthread_mutex_unlock(&g_mutex);
            close(client_socket);
            break;
        }
        
        printf("Received from client %d: %s", client_socket, buffer);
        
        pthread_mutex_lock(&g_mutex);
        for (int i = 0; i < g_count; i++) {
            if (g_sockets[i] != 0 && g_sockets[i] != client_socket) {
                send(g_sockets[i], buffer, received, 0);
            }
        }
        pthread_mutex_unlock(&g_mutex);
    }
    
    return NULL;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    SOCKADDR_IN server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    
    if (bind(server_socket, (SOCKADDR*)&server_addr, sizeof(SOCKADDR)) != 0) {
        printf("Failed to bind!\n");
        close(server_socket);
        return 1;
    }
    
    listen(server_socket, 10);
    printf("Server listening on port 9999...\n");
    
    while (1) {
        SOCKADDR_IN client_addr;
        socklen_t client_len = sizeof(SOCKADDR_IN);
        
        printf("Waiting for connection...\n");
        int client_socket = accept(server_socket, (SOCKADDR*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            printf("Accept failed\n");
            continue;
        }
        
        printf("Client connected: %d\n", client_socket);
        
        pthread_mutex_lock(&g_mutex);
        g_sockets[g_count] = client_socket;
        int current_index = g_count;
        g_count++;
        pthread_mutex_unlock(&g_mutex);
        
        ThreadArg* arg = (ThreadArg*)malloc(sizeof(ThreadArg));
        arg->socket = client_socket;
        arg->index = current_index;
        
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_handler, arg);
        pthread_detach(thread_id);
    }
    
    close(server_socket);
    pthread_mutex_destroy(&g_mutex);
    return 0;
}
