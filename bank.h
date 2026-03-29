#ifndef BANK_H
#define BANK_H

#include <pthread.h>
#include <cstddef>

#define BANK_SHM_NAME "/transparent_bank"
#define DEFAULT_MAX_BALANCE 1000000L

struct Account {
    long balance;
    long min_balance;
    long max_balance;
    int frozen;
};

struct Bank {
    int num_accounts;
    pthread_rwlock_t lock;
    Account accounts[];
};

// Shared memory helpers
size_t bank_shm_size(int num_accounts);
Bank *bank_create(int num_accounts);
Bank *bank_open();
void bank_destroy(Bank *bank);
void bank_close(Bank *bank, int num_accounts);

// Operations (return 0 on success, -1 on error; msg gets filled with result)
int bank_get_balance(Bank *bank, int acc, char *msg, size_t msg_size);
int bank_get_min(Bank *bank, int acc, char *msg, size_t msg_size);
int bank_get_max(Bank *bank, int acc, char *msg, size_t msg_size);
int bank_freeze(Bank *bank, int acc, char *msg, size_t msg_size);
int bank_unfreeze(Bank *bank, int acc, char *msg, size_t msg_size);
int bank_transfer(Bank *bank, int from, int to, long amount, char *msg, size_t msg_size);
int bank_deposit_all(Bank *bank, long amount, char *msg, size_t msg_size);
int bank_withdraw_all(Bank *bank, long amount, char *msg, size_t msg_size);
int bank_set_min(Bank *bank, int acc, long val, char *msg, size_t msg_size);
int bank_set_max(Bank *bank, int acc, long val, char *msg, size_t msg_size);

// Command parser: parses a line, executes, writes response to msg
void bank_execute(Bank *bank, const char *cmd, char *msg, size_t msg_size);

#endif
