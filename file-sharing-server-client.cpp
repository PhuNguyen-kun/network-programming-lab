#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

#define REG_PORT 5000
#define LIST_PORT 6000
#define SEND_PORT 7000
#define BROADCAST_IP "255.255.255.255"

struct ClientInfo {
    std::string name;
    std::string ip;
};

std::map<std::string, std::string> clients;

void broadcast_message(const std::string& message, int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;
    
    int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, BROADCAST_IP, &addr.sin_addr);
    
    sendto(sock, message.c_str(), message.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
}

void run_server() {
    int reg_sock = socket(AF_INET, SOCK_DGRAM, 0);
    int list_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in reg_addr{}, list_addr{};
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(REG_PORT);
    reg_addr.sin_addr.s_addr = INADDR_ANY;
    
    list_addr.sin_family = AF_INET;
    list_addr.sin_port = htons(LIST_PORT);
    list_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(reg_sock, (struct sockaddr*)&reg_addr, sizeof(reg_addr));
    bind(list_sock, (struct sockaddr*)&list_addr, sizeof(list_addr));
    
    std::cout << "Server running on ports " << REG_PORT << " and " << LIST_PORT << std::endl;
    
    fd_set rfds;
    int maxfd = std::max(reg_sock, list_sock);
    
    while (true) {
        FD_ZERO(&rfds);
        FD_SET(reg_sock, &rfds);
        FD_SET(list_sock, &rfds);
        
        select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        
        if (FD_ISSET(reg_sock, &rfds)) {
            char buffer[1024] = {0};
            struct sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            
            recvfrom(reg_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
            
            std::string msg(buffer);
            if (msg.substr(0, 4) == "REG ") {
                std::string name = msg.substr(4);
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
                clients[name] = std::string(ip);
                std::cout << "Registered: " << name << " from " << ip << std::endl;
            }
        }
        
        if (FD_ISSET(list_sock, &rfds)) {
            char buffer[1024] = {0};
            struct sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            
            recvfrom(list_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
            
            std::string msg(buffer);
            if (msg == "LIST") {
                std::ostringstream response;
                response << "LIST " << clients.size();
                for (const auto& client : clients) {
                    response << " " << client.first << " " << client.second;
                }
                
                std::string resp_str = response.str();
                sendto(list_sock, resp_str.c_str(), resp_str.length(), 0, 
                       (struct sockaddr*)&client_addr, len);
            }
        }
    }
}

void run_client() {
    srand(time(nullptr));
    std::string my_name = "uname_" + std::to_string(rand() % 10000);
    std::cout << "My name: " << my_name << std::endl;
    
    std::string reg_msg = "REG " + my_name;
    broadcast_message(reg_msg, REG_PORT);
    std::cout << "Broadcasted registration" << std::endl;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in send_addr{};
        send_addr.sin_family = AF_INET;
        send_addr.sin_port = htons(SEND_PORT);
        send_addr.sin_addr.s_addr = INADDR_ANY;
        
        bind(send_sock, (struct sockaddr*)&send_addr, sizeof(send_addr));
        
        std::cout << "Child process listening on port " << SEND_PORT << std::endl;
        
        while (true) {
            char buffer[1024] = {0};
            struct sockaddr_in sender_addr{};
            socklen_t len = sizeof(sender_addr);
            
            recvfrom(send_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &len);
            
            std::string msg(buffer);
            std::istringstream iss(msg);
            std::string cmd, filename, target_name;
            iss >> cmd >> filename >> target_name;
            
            if (cmd == "SEND" && target_name == my_name) {
                int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in tcp_addr{};
                tcp_addr.sin_family = AF_INET;
                tcp_addr.sin_port = 0;
                tcp_addr.sin_addr.s_addr = INADDR_ANY;
                
                bind(tcp_sock, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr));
                
                struct sockaddr_in local_addr{};
                socklen_t addr_len = sizeof(local_addr);
                getsockname(tcp_sock, (struct sockaddr*)&local_addr, &addr_len);
                int port = ntohs(local_addr.sin_port);
                
                listen(tcp_sock, 1);
                
                char my_ip[INET_ADDRSTRLEN] = "127.0.0.1";
                std::string response = std::string(my_ip) + " " + std::to_string(port);
                sendto(send_sock, response.c_str(), response.length(), 0, 
                       (struct sockaddr*)&sender_addr, len);
                
                std::cout << "Waiting for file on port " << port << std::endl;
                
                int client_fd = accept(tcp_sock, nullptr, nullptr);
                
                uint32_t file_size;
                recv(client_fd, &file_size, 4, 0);
                file_size = ntohl(file_size);
                
                std::string recv_filename = "received_" + filename;
                FILE* fp = fopen(recv_filename.c_str(), "wb");
                
                uint32_t received = 0;
                char file_buffer[4096];
                while (received < file_size) {
                    int n = recv(client_fd, file_buffer, std::min(sizeof(file_buffer), (size_t)(file_size - received)), 0);
                    if (n <= 0) break;
                    fwrite(file_buffer, 1, n, fp);
                    received += n;
                }
                
                fclose(fp);
                close(client_fd);
                close(tcp_sock);
                
                std::cout << "File received: " << recv_filename << " (" << received << " bytes)" << std::endl;
            }
        }
    } else {
        sleep(1);
        
        while (true) {
            std::cout << "\nEnter command (LIST or SEND <file> <name>): ";
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "LIST") {
                broadcast_message("LIST", LIST_PORT);
                
                int list_sock = socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in list_addr{};
                list_addr.sin_family = AF_INET;
                list_addr.sin_port = htons(LIST_PORT + 1000);
                list_addr.sin_addr.s_addr = INADDR_ANY;
                
                bind(list_sock, (struct sockaddr*)&list_addr, sizeof(list_addr));
                
                struct timeval tv;
                tv.tv_sec = 2;
                tv.tv_usec = 0;
                setsockopt(list_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                
                char buffer[4096] = {0};
                recvfrom(list_sock, buffer, sizeof(buffer), 0, nullptr, nullptr);
                
                close(list_sock);
                
                std::string response(buffer);
                std::istringstream iss(response);
                std::string cmd;
                int count;
                iss >> cmd >> count;
                
                std::cout << "\nClients in network:" << std::endl;
                for (int i = 0; i < count; i++) {
                    std::string name, ip;
                    iss >> name >> ip;
                    std::cout << (i + 1) << " " << name << " " << ip << std::endl;
                }
            } else if (input.substr(0, 5) == "SEND ") {
                std::istringstream iss(input);
                std::string cmd, filename, target_name;
                iss >> cmd >> filename >> target_name;
                
                broadcast_message(input, SEND_PORT);
                
                int response_sock = socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in response_addr{};
                response_addr.sin_family = AF_INET;
                response_addr.sin_port = htons(SEND_PORT + 1000);
                response_addr.sin_addr.s_addr = INADDR_ANY;
                
                bind(response_sock, (struct sockaddr*)&response_addr, sizeof(response_addr));
                
                struct timeval tv;
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                setsockopt(response_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                
                char buffer[1024] = {0};
                int n = recvfrom(response_sock, buffer, sizeof(buffer), 0, nullptr, nullptr);
                
                close(response_sock);
                
                if (n > 0) {
                    std::string response(buffer);
                    std::istringstream resp_iss(response);
                    std::string target_ip;
                    int target_port;
                    resp_iss >> target_ip >> target_port;
                    
                    std::cout << "Connecting to " << target_ip << ":" << target_port << std::endl;
                    
                    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in tcp_addr{};
                    tcp_addr.sin_family = AF_INET;
                    tcp_addr.sin_port = htons(target_port);
                    inet_pton(AF_INET, target_ip.c_str(), &tcp_addr.sin_addr);
                    
                    if (connect(tcp_sock, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) == 0) {
                        FILE* fp = fopen(filename.c_str(), "rb");
                        if (fp) {
                            fseek(fp, 0, SEEK_END);
                            uint32_t file_size = ftell(fp);
                            fseek(fp, 0, SEEK_SET);
                            
                            uint32_t network_size = htonl(file_size);
                            send(tcp_sock, &network_size, 4, 0);
                            
                            char file_buffer[4096];
                            size_t bytes_read;
                            while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), fp)) > 0) {
                                send(tcp_sock, file_buffer, bytes_read, 0);
                            }
                            
                            fclose(fp);
                            std::cout << "File sent successfully (" << file_size << " bytes)" << std::endl;
                        } else {
                            std::cout << "Error: Cannot open file " << filename << std::endl;
                        }
                    } else {
                        std::cout << "Error: Cannot connect to target" << std::endl;
                    }
                    
                    close(tcp_sock);
                } else {
                    std::cout << "Error: No response from target" << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [server|client]" << std::endl;
        return 1;
    }
    
    std::string mode(argv[1]);
    
    if (mode == "server") {
        run_server();
    } else if (mode == "client") {
        run_client();
    } else {
        std::cout << "Invalid mode. Use 'server' or 'client'" << std::endl;
        return 1;
    }
    
    return 0;
}
