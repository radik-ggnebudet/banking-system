#include "bank.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

int main() {
    Bank *bank = bank_open();
    if (!bank) {
        fprintf(stderr, "Error: bank not found. Run bank_init first.\n");
        return 1;
    }

    int n = bank->num_accounts;
    int interactive = isatty(STDIN_FILENO);

    if (interactive) {
        printf("Connected to bank with %d accounts\n", n);
        printf("Type 'help' for commands, 'exit' to quit\n");
        printf("> ");
        fflush(stdout);
    }

    char msg[1024];
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line == "exit") break;
        if (line.empty()) {
            if (interactive) { printf("> "); fflush(stdout); }
            continue;
        }

        bank_execute(bank, line.c_str(), msg, sizeof(msg));
        printf("%s\n", msg);
        if (interactive) { printf("> "); fflush(stdout); }
    }

    bank_close(bank, n);
    if (interactive) printf("Bye!\n");
    return 0;
}
