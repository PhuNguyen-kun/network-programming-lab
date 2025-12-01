#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>  
#include <arpa/inet.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#define MY_SIGNAL SIGRTMIN + 1
#define MAX_CLIENTS 1024

typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

typedef struct {
    int sockets[MAX_CLIENTS];
    int count;
} SharedData;

SharedData* g_shared = NULL;
int s = 0;
int parentPID = 0;

void sig_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("Ctrl+C!\n");
        close(s);
        exit(0);
    }
    if (sig == SIGCHLD)
    {
        printf("A child process has been terminated!\n");
    }
    if (sig == MY_SIGNAL)
    {
        printf("A signal from a child process\n");
    }
}

int main()
{
    g_shared = (SharedData*)mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_shared == MAP_FAILED)
    {
        printf("Failed to create shared memory!\n");
        return 1;
    }
    g_shared->count = 0;
    
    parentPID = getpid();
    signal(SIGINT, sig_handler);
    signal(SIGCHLD, sig_handler);
    signal(MY_SIGNAL, sig_handler);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8888);
    saddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    if (bind(s, (SOCKADDR*)&saddr, sizeof(SOCKADDR)) == 0)
    {
        listen(s, 10);
        while (0 == 0)
        {
            SOCKADDR_IN caddr;
            socklen_t clen = sizeof(SOCKADDR_IN);
            printf("Parent: waiting for connection!\n");
            int d = accept(s, (SOCKADDR*)&caddr, &clen);
            printf("Parent: one more client connected!\n");
            g_shared->sockets[g_shared->count++] = d;
            if (fork() == 0)
            {
                kill(parentPID, MY_SIGNAL);
                close(s);
                int my_socket = d;
                while (0 == 0)
                {
                    char buffer[1024] = { 0 };
                    int received = recv(d, buffer, sizeof(buffer) - 1, 0);
                    if (received > 0)
                    {
                        printf("Child: received %d bytes: %s\n", received, buffer);
                        for (int i = 0; i < g_shared->count; i++)
                        {
                            if (g_shared->sockets[i] != my_socket && g_shared->sockets[i] > 0)
                            {
                                send(g_shared->sockets[i], buffer, received, 0);
                            }
                        }
                    }else
                    {
                        printf("Child: disconnected!\n");
                        for (int i = 0; i < g_shared->count; i++)
                        {
                            if (g_shared->sockets[i] == my_socket)
                            {
                                g_shared->sockets[i] = -1;
                                break;
                            }
                        }
                        exit(0);
                    }
                }
            }
            close(d);
        }
    }else
    {
        printf("Failed to bind!\n");
        close(s);
    }
    
    munmap(g_shared, sizeof(SharedData));
}