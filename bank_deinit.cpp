#include "bank.h"
#include <cstdio>

int main() {
    Bank *bank = bank_open();
    if (!bank) {
        fprintf(stderr, "Error: bank not found or cannot open\n");
        return 1;
    }

    bank_destroy(bank);
    printf("Bank destroyed\n");
    return 0;
}
