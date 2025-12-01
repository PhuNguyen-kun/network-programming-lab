#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

struct ClientInfo {
    struct sockaddr_in addr;
    
    bool operator==(const ClientInfo& other) const {
        return addr.sin_addr.s_addr == other.addr.sin_addr.s_addr &&
               addr.sin_port == other.addr.sin_port;
    }
};

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5000);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    std::cout << "UDP Chatroom Server listening on port 5000...\n";

    std::vector<ClientInfo> clients;
    char buffer[4096];

    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr*)&client_addr, &client_len);
        
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        buffer[n] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        ClientInfo current_client;
        current_client.addr = client_addr;

        auto it = std::find(clients.begin(), clients.end(), current_client);
        if (it == clients.end()) {
            clients.push_back(current_client);
            std::cout << "New client added: " << client_ip << ":" << client_port 
                      << " (Total clients: " << clients.size() << ")\n";
        }

        std::cout << "Received from " << client_ip << ":" << client_port 
                  << ": " << buffer;

        std::string message = std::string("[") + client_ip + ":" + 
                             std::to_string(client_port) + "] " + buffer;

        for (const auto& client : clients) {
            if (!(client == current_client)) {
                sendto(sockfd, message.c_str(), message.length(), 0,
                       (struct sockaddr*)&client.addr, sizeof(client.addr));
            }
        }

        std::cout << "Message forwarded to " << (clients.size() - 1) << " other client(s)\n";
    }

    close(sockfd);
    return 0;
}
