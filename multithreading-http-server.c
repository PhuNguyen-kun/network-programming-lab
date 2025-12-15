#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <sys/stat.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define ROOT_FOLDER "."
#define BUFFER_SIZE 8192

typedef struct {
    char name[256];
    int is_dir;
} FileEntry;

int compare_entries(const void* a, const void* b) {
    FileEntry* fa = (FileEntry*)a;
    FileEntry* fb = (FileEntry*)b;
    
    if (fa->is_dir != fb->is_dir)
        return fb->is_dir - fa->is_dir;
    
    return strcmp(fa->name, fb->name);
}

void url_encode(const char* src, char* dst, size_t dst_size) {
    const char* hex = "0123456789ABCDEF";
    size_t pos = 0;
    
    while (*src && pos < dst_size - 4) {
        if ((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z') ||
            (*src >= '0' && *src <= '9') || *src == '-' || *src == '_' ||
            *src == '.' || *src == '~' || *src == '/') {
            dst[pos++] = *src;
        } else {
            dst[pos++] = '%';
            dst[pos++] = hex[(*src >> 4) & 0x0F];
            dst[pos++] = hex[*src & 0x0F];
        }
        src++;
    }
    dst[pos] = '\0';
}

void scan_directory(char* html_buffer, size_t buffer_size) {
    WIN32_FIND_DATA find_data;
    HANDLE hFind;
    
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", ROOT_FOLDER);
    
    hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        snprintf(html_buffer, buffer_size, 
                "<html><body><h1>Error opening directory</h1></body></html>");
        return;
    }
    
    FileEntry entries[1024];
    int count = 0;
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;
        
        if (count < 1024) {
            strncpy(entries[count].name, find_data.cFileName, 255);
            entries[count].name[255] = '\0';
            entries[count].is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            count++;
        }
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
    
    qsort(entries, count, sizeof(FileEntry), compare_entries);
    
    char* ptr = html_buffer;
    size_t remaining = buffer_size;
    int written;
    
    written = snprintf(ptr, remaining,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>File Browser</title>\n"
        "    <link rel=\"icon\" href=\"/favicon.ico\" type=\"image/x-icon\">\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 20px; }\n"
        "        h1 { color: #333; }\n"
        "        ul { list-style-type: none; padding: 0; }\n"
        "        li { margin: 5px 0; }\n"
        "        a { text-decoration: none; color: #0066cc; }\n"
        "        a:hover { text-decoration: underline; }\n"
        "        .folder { font-weight: bold; }\n"
        "        .file { font-style: italic; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Directory Listing</h1>\n"
        "    <ul>\n");
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < count; i++) {
        char encoded_name[512];
        url_encode(entries[i].name, encoded_name, sizeof(encoded_name));
        
        if (entries[i].is_dir) {
            written = snprintf(ptr, remaining,
                "        <li class=\"folder\"><a href=\"/files/%s/\">%s/</a></li>\n",
                encoded_name, entries[i].name);
        } else {
            written = snprintf(ptr, remaining,
                "        <li class=\"file\"><a href=\"/files/%s\">%s</a></li>\n",
                encoded_name, entries[i].name);
        }
        ptr += written;
        remaining -= written;
    }
    
    written = snprintf(ptr, remaining,
        "    </ul>\n"
        "</body>\n"
        "</html>\n");
}

void send_favicon(SOCKET client_socket) {
    unsigned char favicon[] = {
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x00, 0x00, 0x01, 0x00,
        0x18, 0x00, 0x68, 0x03, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x4169, 0xE1, 0x41, 0x69, 0xE1, 0x41, 0x69,
        0xE1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    int favicon_size = sizeof(favicon);
    
    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/x-icon\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        favicon_size);
    
    send(client_socket, header, header_len, 0);
    send(client_socket, (char*)favicon, favicon_size, 0);
}

void send_response(SOCKET client_socket, char* html_content) {
    char response[BUFFER_SIZE];
    int content_length = strlen(html_content);
    
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        content_length, html_content);
    
    send(client_socket, response, (int)strlen(response), 0);
}

DWORD WINAPI handle_client(LPVOID arg) {
    SOCKET client_socket = *(SOCKET*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';
        
        char method[16], path[512];
        sscanf(buffer, "%s %s", method, path);
        printf("Request: %s %s\n", method, path);
        
        if (strcmp(path, "/favicon.ico") == 0) {
            send_favicon(client_socket);
        } else {
            char html_content[BUFFER_SIZE];
            scan_directory(html_content, sizeof(html_content));
            send_response(client_socket, html_content);
        }
    }
    
    closesocket(client_socket);
    return 0;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    if (listen(server_socket, 10) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    printf("HTTP Server listening on port %d...\n", PORT);
    printf("Open http://localhost:%d in your browser\n", PORT);
    
    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed\n");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        SOCKET* client_sock_ptr = malloc(sizeof(SOCKET));
        *client_sock_ptr = client_socket;
        
        HANDLE thread = CreateThread(NULL, 0, handle_client, client_sock_ptr, 0, NULL);
        if (thread) {
            CloseHandle(thread);
        }
    }
    
    closesocket(server_socket);
    WSACleanup();
    return 0;
}
