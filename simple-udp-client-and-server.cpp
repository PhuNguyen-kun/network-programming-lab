#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

void udp_server() {
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Server socket creation failed");
        return;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5000);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Server bind failed");
        close(server_fd);
        return;
    }

    std::cout << "UDP Server is listening on port 5000...\n";

    while (true) {
        char buffer[1024] = {0};
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        ssize_t bytes_received = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr*)&client_addr, &client_len);

        if (bytes_received < 0) {
            perror("recvfrom failed");
            continue;
        }

        buffer[bytes_received] = '\0';
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Received from " << client_ip << ": " << buffer;

        client_addr.sin_port = htons(6000);

        ssize_t bytes_sent = sendto(server_fd, buffer, bytes_received, 0,
                                    (struct sockaddr*)&client_addr, client_len);

        if (bytes_sent < 0) {
            perror("sendto failed");
        } else {
            std::cout << "Sent back to client on port 6000\n";
        }
    }

    close(server_fd);
}

void udp_client() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        perror("Client socket creation failed");
        return;
    }

    struct sockaddr_in client_addr{};
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(6000);

    if (bind(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Client bind failed");
        close(client_fd);
        return;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(5000);

    std::cout << "UDP Client ready. Enter text to send to server (Ctrl+D to quit):\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        line += "\n";

        ssize_t bytes_sent = sendto(client_fd, line.c_str(), line.length(), 0,
                                    (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (bytes_sent < 0) {
            perror("sendto failed");
            continue;
        }

        char buffer[1024] = {0};
        struct sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);

        ssize_t bytes_received = recvfrom(client_fd, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr*)&from_addr, &from_len);

        if (bytes_received < 0) {
            perror("recvfrom failed");
            continue;
        }

        buffer[bytes_received] = '\0';
        std::cout << "Response from server: " << buffer;
    }

    close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server|client|both>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "server") {
        udp_server();
    } else if (mode == "client") {
        udp_client();
    } else if (mode == "both") {
        std::thread server_thread(udp_server);
        std::thread client_thread(udp_client);
        
        client_thread.join();
        server_thread.detach();
    } else {
        std::cerr << "Invalid mode. Use 'server', 'client', or 'both'\n";
        return 1;
    }

    return 0;
}
