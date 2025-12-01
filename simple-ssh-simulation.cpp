#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <array>
#include <memory>

std::string exec_command(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "Error executing command";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9999); 

    if (bind(server_fd, (struct sockaddr*)&address, sizeof( address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    std::cout << "SSH Simulation Server is listening on port 9999...\n";

    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        
        std::cout << "New connection from " << client_ip << ":" << client_port << std::endl;

        const char* welcome_msg = "Welcome to SSH Simulation. Enter commands to execute (type 'exit' to quit):\n";
        send(client_fd, welcome_msg, strlen(welcome_msg), 0);

        bool client_connected = true;
        while (client_connected) {
            char buffer[4096] = {0};
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read <= 0) {
                std::cout << "Client disconnected\n";
                break;
            }
            
            buffer[bytes_read] = '\0';
            
            if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
                buffer[bytes_read - 1] = '\0';
            }
            
            std::string command(buffer);
            std::cout << "Received command: " << command << std::endl;
            
            if (command == "exit") {
                const char* exit_msg = "Closing connection. Goodbye!\n";
                send(client_fd, exit_msg, strlen(exit_msg), 0);
                client_connected = false;
                continue;
            }
            
            std::string cmd_output;
            
            if (command.find("rm -rf") != std::string::npos || 
                command.find(":(){ :|:& };:") != std::string::npos) {
                cmd_output = "Error: Potentially dangerous command blocked\n";
            } else {
                cmd_output = exec_command(command);
                
                if (cmd_output.empty()) {
                    cmd_output = "Command executed successfully (no output)\n";
                }
            }
            
            send(client_fd, cmd_output.c_str(), cmd_output.length(), 0);
        }

        close(client_fd);
        std::cout << "Connection closed, waiting for a new client...\n" << std::endl;
    }

    close(server_fd);
    return 0;
}
