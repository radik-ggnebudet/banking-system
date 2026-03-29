#include "bank.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_accounts>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "Error: number of accounts must be positive\n");
        return 1;
    }

    Bank *bank = bank_create(n);
    if (!bank) {
        perror("Failed to create bank");
        return 1;
    }

    printf("Bank created with %d accounts (max balance: %ld)\n", n, (long)DEFAULT_MAX_BALANCE);
    bank_close(bank, n);
    return 0;
}
