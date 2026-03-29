#include "colorprint.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int interactive = isatty(STDIN_FILENO);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: invalid address '%s'\n", ip);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    Painter painter(std::cout,
        {"successfully", "Transferred", "Deposited", "Withdrew", "frozen", "unfrozen", "set to"},
        {"Error"}
    );

    if (interactive) {
        printf("Connected to server %s:%d\n", ip, port);
        printf("Type 'help' for commands, 'exit' to quit\n");
        printf("> ");
        fflush(stdout);
    }

    std::string line;
    char buf[1024];

    while (std::getline(std::cin, line)) {
        if (line == "exit") break;
        if (line.empty()) {
            if (interactive) { printf("> "); fflush(stdout); }
            continue;
        }

        // send command
        std::string to_send = line + "\n";
        send(sock, to_send.c_str(), to_send.size(), 0);

        // receive response
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (interactive) printf("Server disconnected\n");
            break;
        }
        buf[n] = '\0';
        // remove trailing newline for display
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';

        if (interactive) {
            painter.print(buf);
        } else {
            printf("%s\n", buf);
        }

        if (line == "shutdown") break;

        if (interactive) { printf("> "); fflush(stdout); }
    }

    close(sock);
    if (interactive) printf("Bye!\n");
    return 0;
}
