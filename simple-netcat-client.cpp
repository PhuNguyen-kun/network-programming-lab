#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
int main(int argc, char* argv[]) {
    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? std::atoi(argv[2]) : 5000;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "Invalid host: " << host << std::endl;
        close(sock);
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    std::cerr << "✅ Connected to " << host << ":" << port << std::endl;
    bool stdin_closed = false;
    while (true) {
        fd_set rfds;
        FD_ZERO(&rfds);
        if (!stdin_closed) FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sock, &rfds);
        int maxfd = std::max(sock, STDIN_FILENO);
        int rc = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (!stdin_closed && FD_ISSET(STDIN_FILENO, &rfds)) {
            char buf[4096];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                stdin_closed = true;
                shutdown(sock, SHUT_WR);
            } else {
                ssize_t off = 0;
                while (off < n) {
                    ssize_t m = send(sock, buf + off, n - off, 0);
                    if (m < 0) {
                        perror("send");
                        goto end;
                    }
                    off += m;
                }
            }
        }
        if (FD_ISSET(sock, &rfds)) {
            char buf[4096];
            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            ssize_t off = 0;
            while (off < n) {
                ssize_t m = write(STDOUT_FILENO, buf + off, n - off);
                if (m < 0) {
                    perror("write");
                    goto end;
                }
                off += m;
            }
        }
    }
end:
    close(sock);
    return 0;
}