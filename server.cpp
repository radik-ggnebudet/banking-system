#include "bank.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

static Bank *g_bank = nullptr;
static int g_listen_fd = -1;
static volatile int g_shutdown = 0;

// stats
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_stats_cond = PTHREAD_COND_INITIALIZER;
static int g_total_requests = 0;

static void increment_requests() {
    pthread_mutex_lock(&g_stats_mutex);
    g_total_requests++;
    if (g_total_requests % 5 == 0) {
        pthread_cond_signal(&g_stats_cond);
    }
    pthread_mutex_unlock(&g_stats_mutex);
}

// stats thread
static void *stats_thread(void *) {
    pthread_mutex_lock(&g_stats_mutex);
    while (!g_shutdown) {
        pthread_cond_wait(&g_stats_cond, &g_stats_mutex);
        printf("[Stats] Total requests processed: %d\n", g_total_requests);
        fflush(stdout);
    }
    pthread_mutex_unlock(&g_stats_mutex);
    return nullptr;
}

// client handler thread
static void *client_handler(void *arg) {
    int client_fd = (int)(long)arg;
    char buf[1024];
    char msg[1024];

    while (!g_shutdown) {
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        buf[n] = '\0';
        // remove trailing newline
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';

        if (strlen(buf) == 0) continue;

        if (strcmp(buf, "shutdown") == 0) {
            snprintf(msg, sizeof(msg), "Server shutting down...\n");
            send(client_fd, msg, strlen(msg), 0);
            g_shutdown = 1;
            // close listen socket to unblock accept()
            if (g_listen_fd >= 0) {
                shutdown(g_listen_fd, SHUT_RDWR);
                close(g_listen_fd);
                g_listen_fd = -1;
            }
            // wake stats thread
            pthread_mutex_lock(&g_stats_mutex);
            pthread_cond_signal(&g_stats_cond);
            pthread_mutex_unlock(&g_stats_mutex);
            break;
        }

        bank_execute(g_bank, buf, msg, sizeof(msg));
        increment_requests();

        // send response with newline
        strcat(msg, "\n");
        send(client_fd, msg, strlen(msg), 0);
    }

    close(client_fd);
    return nullptr;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_accounts> <port>\n", argv[0]);
        return 1;
    }

    int num_accounts = atoi(argv[1]);
    int port = atoi(argv[2]);

    if (num_accounts <= 0 || port <= 0) {
        fprintf(stderr, "Error: invalid arguments\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    g_bank = bank_create(num_accounts);
    if (!g_bank) {
        perror("Failed to create bank");
        return 1;
    }
    printf("Bank created with %d accounts\n", num_accounts);

    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(g_listen_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d\n", port);

    // start stats thread
    pthread_t stats_tid;
    pthread_create(&stats_tid, nullptr, stats_thread, nullptr);
    pthread_detach(stats_tid);

    while (!g_shutdown) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (g_shutdown) break;
            perror("accept");
            continue;
        }

        printf("Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, nullptr, client_handler, (void *)(long)client_fd);
        pthread_detach(tid);
    }

    printf("Server shutting down\n");
    bank_destroy(g_bank);
    _exit(0);
}
