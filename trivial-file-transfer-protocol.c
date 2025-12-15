#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib")

#define TFTP_PORT 69
#define BLOCK_SIZE 512
#define BUFFER_SIZE 516
#define TIMEOUT_SEC 5000
#define MAX_RETRIES 5

#define OP_RRQ   1
#define OP_WRQ   2
#define OP_DATA  3
#define OP_ACK   4
#define OP_ERROR 5

typedef struct {
    uint16_t opcode;
    union {
        struct {
            char filename_and_mode[512];
        } request;
        struct {
            uint16_t block_num;
            char data[BLOCK_SIZE];
        } data;
        struct {
            uint16_t block_num;
        } ack;
        struct {
            uint16_t error_code;
            char error_msg[512];
        } error;
    };
} TFTPPacket;

void print_usage(char* prog_name) {
    printf("Usage:\n");
    printf("  %s get <server_ip> <remote_file> [local_file]\n", prog_name);
    printf("  %s put <server_ip> <local_file> [remote_file]\n", prog_name);
    printf("\nExamples:\n");
    printf("  %s get 192.168.1.100 test.txt\n", prog_name);
    printf("  %s put 192.168.1.100 myfile.txt server_file.txt\n", prog_name);
}

void send_error(SOCKET sock, struct sockaddr_in* server_addr, uint16_t error_code, char* error_msg) {
    char buffer[BUFFER_SIZE];
    uint16_t* opcode = (uint16_t*)buffer;
    uint16_t* errcode = (uint16_t*)(buffer + 2);
    
    *opcode = htons(OP_ERROR);
    *errcode = htons(error_code);
    strcpy(buffer + 4, error_msg);
    
    sendto(sock, buffer, 4 + (int)strlen(error_msg) + 1, 0,
           (struct sockaddr*)server_addr, sizeof(*server_addr));
}

int tftp_get(char* server_ip, char* remote_file, char* local_file) {
    SOCKET sock;
    struct sockaddr_in server_addr, from_addr;
    int from_len = sizeof(from_addr);
    char buffer[BUFFER_SIZE];
    FILE* fp;
    DWORD timeout = TIMEOUT_SEC;
    int retries = 0;
    uint16_t expected_block = 1;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return -1;
    }
    
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TFTP_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    fp = fopen(local_file, "wb");
    if (!fp) {
        printf("Cannot open file %s for writing\n", local_file);
        closesocket(sock);
        return -1;
    }
    
    uint16_t* opcode = (uint16_t*)buffer;
    *opcode = htons(OP_RRQ);
    
    char* ptr = buffer + 2;
    strcpy(ptr, remote_file);
    ptr += strlen(remote_file) + 1;
    strcpy(ptr, "octet");
    ptr += strlen("octet") + 1;
    
    int request_len = ptr - buffer;
    
    printf("Requesting file '%s' from %s...\n", remote_file, server_ip);
    sendto(sock, buffer, request_len, 0,
           (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    while (1) {
        int n = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                        (struct sockaddr*)&from_addr, &from_len);
        
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (retries++ < MAX_RETRIES) {
                    printf("Timeout, retrying (%d/%d)...\n", retries, MAX_RETRIES);
                    
                    char ack_buffer[4];
                    uint16_t* ack_opcode = (uint16_t*)ack_buffer;
                    uint16_t* ack_block = (uint16_t*)(ack_buffer + 2);
                    *ack_opcode = htons(OP_ACK);
                    *ack_block = htons(expected_block - 1);
                    
                    sendto(sock, ack_buffer, 4, 0,
                           (struct sockaddr*)&from_addr, sizeof(from_addr));
                    continue;
                } else {
                    printf("Max retries reached. Download failed.\n");
                    fclose(fp);
                    closesocket(sock);
                    return -1;
                }
            } else {
                printf("Receive error: %d\n", WSAGetLastError());
                fclose(fp);
                closesocket(sock);
                return -1;
            }
        }
        
        retries = 0;
        
        uint16_t recv_opcode = ntohs(*(uint16_t*)buffer);
        
        if (recv_opcode == OP_ERROR) {
            uint16_t error_code = ntohs(*(uint16_t*)(buffer + 2));
            char* error_msg = buffer + 4;
            printf("Error %d: %s\n", error_code, error_msg);
            fclose(fp);
            closesocket(sock);
            return -1;
        }
        
        if (recv_opcode == OP_DATA) {
            uint16_t block_num = ntohs(*(uint16_t*)(buffer + 2));
            int data_len = n - 4;
            
            if (block_num == expected_block) {
                fwrite(buffer + 4, 1, data_len, fp);
                printf("Received block %d (%d bytes)\n", block_num, data_len);
                
                char ack_buffer[4];
                uint16_t* ack_opcode = (uint16_t*)ack_buffer;
                uint16_t* ack_block = (uint16_t*)(ack_buffer + 2);
                *ack_opcode = htons(OP_ACK);
                *ack_block = htons(block_num);
                
                sendto(sock, ack_buffer, 4, 0,
                       (struct sockaddr*)&from_addr, sizeof(from_addr));
                
                expected_block++;
                
                if (data_len < BLOCK_SIZE) {
                    printf("Transfer complete!\n");
                    break;
                }
            } else if (block_num < expected_block) {
                char ack_buffer[4];
                uint16_t* ack_opcode = (uint16_t*)ack_buffer;
                uint16_t* ack_block = (uint16_t*)(ack_buffer + 2);
                *ack_opcode = htons(OP_ACK);
                *ack_block = htons(block_num);
                
                sendto(sock, ack_buffer, 4, 0,
                       (struct sockaddr*)&from_addr, sizeof(from_addr));
            }
        }
    }
    
    fclose(fp);
    closesocket(sock);
    return 0;
}

int tftp_put(char* server_ip, char* local_file, char* remote_file) {
    SOCKET sock;
    struct sockaddr_in server_addr, from_addr;
    int from_len = sizeof(from_addr);
    char buffer[BUFFER_SIZE];
    FILE* fp;
    DWORD timeout = TIMEOUT_SEC;
    int retries = 0;
    uint16_t block_num = 0;
    
    fp = fopen(local_file, "rb");
    if (!fp) {
        printf("Cannot open file %s for reading\n", local_file);
        return -1;
    }
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        fclose(fp);
        return -1;
    }
    
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TFTP_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    uint16_t* opcode = (uint16_t*)buffer;
    *opcode = htons(OP_WRQ);
    
    char* ptr = buffer + 2;
    strcpy(ptr, remote_file);
    ptr += strlen(remote_file) + 1;
    strcpy(ptr, "octet");
    ptr += strlen("octet") + 1;
    
    int request_len = ptr - buffer;
    
    printf("Sending file '%s' to %s as '%s'...\n", local_file, server_ip, remote_file);
    sendto(sock, buffer, request_len, 0,
           (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    while (1) {
        int n = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                        (struct sockaddr*)&from_addr, &from_len);
        
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (retries++ < MAX_RETRIES) {
                    printf("Timeout, retrying (%d/%d)...\n", retries, MAX_RETRIES);
                    
                    if (block_num == 0) {
                        sendto(sock, buffer, request_len, 0,
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
                    } else {
                        sendto(sock, buffer, 4 + (n - 4), 0,
                               (struct sockaddr*)&from_addr, sizeof(from_addr));
                    }
                    continue;
                } else {
                    printf("Max retries reached. Upload failed.\n");
                    fclose(fp);
                    closesocket(sock);
                    return -1;
                }
            } else {
                printf("Receive error: %d\n", WSAGetLastError());
                fclose(fp);
                closesocket(sock);
                return -1;
            }
        }
        
        retries = 0;
        
        uint16_t recv_opcode = ntohs(*(uint16_t*)buffer);
        
        if (recv_opcode == OP_ERROR) {
            uint16_t error_code = ntohs(*(uint16_t*)(buffer + 2));
            char* error_msg = buffer + 4;
            printf("Error %d: %s\n", error_code, error_msg);
            fclose(fp);
            closesocket(sock);
            return -1;
        }
        
        if (recv_opcode == OP_ACK) {
            uint16_t ack_block = ntohs(*(uint16_t*)(buffer + 2));
            
            if (ack_block == block_num) {
                block_num++;
                
                char data_buffer[BUFFER_SIZE];
                uint16_t* data_opcode = (uint16_t*)data_buffer;
                uint16_t* data_block = (uint16_t*)(data_buffer + 2);
                
                *data_opcode = htons(OP_DATA);
                *data_block = htons(block_num);
                
                int bytes_read = fread(data_buffer + 4, 1, BLOCK_SIZE, fp);
                
                if (bytes_read > 0) {
                    printf("Sending block %d (%d bytes)\n", block_num, bytes_read);
                    sendto(sock, data_buffer, 4 + bytes_read, 0,
                           (struct sockaddr*)&from_addr, sizeof(from_addr));
                    
                    memcpy(buffer, data_buffer, 4 + bytes_read);
                    n = 4 + bytes_read;
                }
                
                if (bytes_read < BLOCK_SIZE) {
                    n = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                (struct sockaddr*)&from_addr, &from_len);
                    if (n > 0) {
                        recv_opcode = ntohs(*(uint16_t*)buffer);
                        if (recv_opcode == OP_ACK) {
                            ack_block = ntohs(*(uint16_t*)(buffer + 2));
                            if (ack_block == block_num) {
                                printf("Transfer complete!\n");
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    
    fclose(fp);
    closesocket(sock);
    return 0;
}

int main(int argc, char* argv[]) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    if (argc < 4) {
        print_usage(argv[0]);
        WSACleanup();
        return 1;
    }
    
    char* command = argv[1];
    char* server_ip = argv[2];
    int result = 0;
    
    if (strcmp(command, "get") == 0) {
        char* remote_file = argv[3];
        char* local_file = (argc >= 5) ? argv[4] : remote_file;
        result = tftp_get(server_ip, remote_file, local_file);
    } 
    else if (strcmp(command, "put") == 0) {
        char* local_file = argv[3];
        char* remote_file = (argc >= 5) ? argv[4] : local_file;
        result = tftp_put(server_ip, local_file, remote_file);
    } 
    else {
        print_usage(argv[0]);
        WSACleanup();
        return 1;
    }
    
    WSACleanup();
    return result;
}
