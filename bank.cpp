#include "bank.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

size_t bank_shm_size(int n) {
    return sizeof(Bank) + sizeof(Account) * n;
}

Bank *bank_create(int n) {
    shm_unlink(BANK_SHM_NAME);
    int fd = shm_open(BANK_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) return nullptr;

    size_t sz = bank_shm_size(n);
    if (ftruncate(fd, sz) < 0) { close(fd); return nullptr; }

    Bank *bank = (Bank *)mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (bank == MAP_FAILED) return nullptr;

    bank->num_accounts = n;

    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&bank->lock, &attr);
    pthread_rwlockattr_destroy(&attr);

    for (int i = 0; i < n; i++) {
        bank->accounts[i].balance = 0;
        bank->accounts[i].min_balance = 0;
        bank->accounts[i].max_balance = DEFAULT_MAX_BALANCE;
        bank->accounts[i].frozen = 0;
    }
    return bank;
}

Bank *bank_open() {
    int fd = shm_open(BANK_SHM_NAME, O_RDWR, 0666);
    if (fd < 0) return nullptr;

    // read num_accounts first
    int n;
    if (read(fd, &n, sizeof(int)) != sizeof(int)) { close(fd); return nullptr; }

    size_t sz = bank_shm_size(n);
    Bank *bank = (Bank *)mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (bank == MAP_FAILED) return nullptr;
    return bank;
}

void bank_destroy(Bank *bank) {
    pthread_rwlock_destroy(&bank->lock);
    munmap(bank, bank_shm_size(bank->num_accounts));
    shm_unlink(BANK_SHM_NAME);
}

void bank_close(Bank *bank, int n) {
    munmap(bank, bank_shm_size(n));
}

// --- helpers ---

static bool valid_acc(Bank *bank, int acc) {
    return acc >= 0 && acc < bank->num_accounts;
}

// --- operations ---

int bank_get_balance(Bank *bank, int acc, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_rdlock(&bank->lock);
    long bal = bank->accounts[acc].balance;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Balance of account %d: %ld", acc, bal);
    return 0;
}

int bank_get_min(Bank *bank, int acc, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_rdlock(&bank->lock);
    long val = bank->accounts[acc].min_balance;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Min balance of account %d: %ld", acc, val);
    return 0;
}

int bank_get_max(Bank *bank, int acc, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_rdlock(&bank->lock);
    long val = bank->accounts[acc].max_balance;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Max balance of account %d: %ld", acc, val);
    return 0;
}

int bank_freeze(Bank *bank, int acc, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    if (bank->accounts[acc].frozen) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d is already frozen", acc);
        return -1;
    }
    bank->accounts[acc].frozen = 1;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Account %d frozen successfully", acc);
    return 0;
}

int bank_unfreeze(Bank *bank, int acc, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    if (!bank->accounts[acc].frozen) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d is not frozen", acc);
        return -1;
    }
    bank->accounts[acc].frozen = 0;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Account %d unfrozen successfully", acc);
    return 0;
}

int bank_transfer(Bank *bank, int from, int to, long amount, char *msg, size_t sz) {
    if (!valid_acc(bank, from) || !valid_acc(bank, to)) {
        snprintf(msg, sz, "Error: invalid account number");
        return -1;
    }
    if (from == to) {
        snprintf(msg, sz, "Error: cannot transfer to the same account");
        return -1;
    }
    if (amount <= 0) {
        snprintf(msg, sz, "Error: transfer amount must be positive");
        return -1;
    }

    pthread_rwlock_wrlock(&bank->lock);
    Account &a = bank->accounts[from];
    Account &b = bank->accounts[to];

    if (a.frozen) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d is frozen", from);
        return -1;
    }
    if (b.frozen) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d is frozen", to);
        return -1;
    }

    long new_a = a.balance - amount;
    long new_b = b.balance + amount;

    if (new_a < a.min_balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d balance would go below minimum (%ld)", from, a.min_balance);
        return -1;
    }
    if (new_b > b.max_balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: account %d balance would exceed maximum (%ld)", to, b.max_balance);
        return -1;
    }

    a.balance = new_a;
    b.balance = new_b;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Transferred %ld from account %d to account %d", amount, from, to);
    return 0;
}

int bank_deposit_all(Bank *bank, long amount, char *msg, size_t sz) {
    if (amount <= 0) {
        snprintf(msg, sz, "Error: deposit amount must be positive");
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    // check all first
    for (int i = 0; i < bank->num_accounts; i++) {
        Account &a = bank->accounts[i];
        if (a.frozen) {
            pthread_rwlock_unlock(&bank->lock);
            snprintf(msg, sz, "Error: account %d is frozen, deposit cancelled", i);
            return -1;
        }
        if (a.balance + amount > a.max_balance) {
            pthread_rwlock_unlock(&bank->lock);
            snprintf(msg, sz, "Error: account %d would exceed max balance, deposit cancelled", i);
            return -1;
        }
    }
    for (int i = 0; i < bank->num_accounts; i++) {
        bank->accounts[i].balance += amount;
    }
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Deposited %ld to all accounts", amount);
    return 0;
}

int bank_withdraw_all(Bank *bank, long amount, char *msg, size_t sz) {
    if (amount <= 0) {
        snprintf(msg, sz, "Error: withdrawal amount must be positive");
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    for (int i = 0; i < bank->num_accounts; i++) {
        Account &a = bank->accounts[i];
        if (a.frozen) {
            pthread_rwlock_unlock(&bank->lock);
            snprintf(msg, sz, "Error: account %d is frozen, withdrawal cancelled", i);
            return -1;
        }
        if (a.balance - amount < a.min_balance) {
            pthread_rwlock_unlock(&bank->lock);
            snprintf(msg, sz, "Error: account %d would go below min balance, withdrawal cancelled", i);
            return -1;
        }
    }
    for (int i = 0; i < bank->num_accounts; i++) {
        bank->accounts[i].balance -= amount;
    }
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Withdrew %ld from all accounts", amount);
    return 0;
}

int bank_set_min(Bank *bank, int acc, long val, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    Account &a = bank->accounts[acc];
    if (val > a.balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: min balance %ld exceeds current balance %ld", val, a.balance);
        return -1;
    }
    if (val > a.max_balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: min balance %ld exceeds max balance %ld", val, a.max_balance);
        return -1;
    }
    a.min_balance = val;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Min balance of account %d set to %ld", acc, val);
    return 0;
}

int bank_set_max(Bank *bank, int acc, long val, char *msg, size_t sz) {
    if (!valid_acc(bank, acc)) {
        snprintf(msg, sz, "Error: invalid account %d", acc);
        return -1;
    }
    pthread_rwlock_wrlock(&bank->lock);
    Account &a = bank->accounts[acc];
    if (val < a.balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: max balance %ld is below current balance %ld", val, a.balance);
        return -1;
    }
    if (val < a.min_balance) {
        pthread_rwlock_unlock(&bank->lock);
        snprintf(msg, sz, "Error: max balance %ld is below min balance %ld", val, a.min_balance);
        return -1;
    }
    a.max_balance = val;
    pthread_rwlock_unlock(&bank->lock);
    snprintf(msg, sz, "Max balance of account %d set to %ld", acc, val);
    return 0;
}

// --- command parser ---

void bank_execute(Bank *bank, const char *cmd, char *msg, size_t sz) {
    std::istringstream iss(cmd);
    std::string op;
    if (!(iss >> op)) {
        snprintf(msg, sz, "Error: empty command");
        return;
    }

    if (op == "balance") {
        int a;
        if (!(iss >> a)) { snprintf(msg, sz, "Error: usage: balance <account>"); return; }
        bank_get_balance(bank, a, msg, sz);
    } else if (op == "min") {
        int a;
        if (!(iss >> a)) { snprintf(msg, sz, "Error: usage: min <account>"); return; }
        bank_get_min(bank, a, msg, sz);
    } else if (op == "max") {
        int a;
        if (!(iss >> a)) { snprintf(msg, sz, "Error: usage: max <account>"); return; }
        bank_get_max(bank, a, msg, sz);
    } else if (op == "freeze") {
        int a;
        if (!(iss >> a)) { snprintf(msg, sz, "Error: usage: freeze <account>"); return; }
        bank_freeze(bank, a, msg, sz);
    } else if (op == "unfreeze") {
        int a;
        if (!(iss >> a)) { snprintf(msg, sz, "Error: usage: unfreeze <account>"); return; }
        bank_unfreeze(bank, a, msg, sz);
    } else if (op == "transfer") {
        int a, b; long x;
        if (!(iss >> a >> b >> x)) { snprintf(msg, sz, "Error: usage: transfer <from> <to> <amount>"); return; }
        bank_transfer(bank, a, b, x, msg, sz);
    } else if (op == "deposit") {
        long x;
        if (!(iss >> x)) { snprintf(msg, sz, "Error: usage: deposit <amount>"); return; }
        bank_deposit_all(bank, x, msg, sz);
    } else if (op == "withdraw") {
        long x;
        if (!(iss >> x)) { snprintf(msg, sz, "Error: usage: withdraw <amount>"); return; }
        bank_withdraw_all(bank, x, msg, sz);
    } else if (op == "setmin") {
        int a; long x;
        if (!(iss >> a >> x)) { snprintf(msg, sz, "Error: usage: setmin <account> <value>"); return; }
        bank_set_min(bank, a, x, msg, sz);
    } else if (op == "setmax") {
        int a; long x;
        if (!(iss >> a >> x)) { snprintf(msg, sz, "Error: usage: setmax <account> <value>"); return; }
        bank_set_max(bank, a, x, msg, sz);
    } else if (op == "help") {
        snprintf(msg, sz,
            "Commands:\n"
            "  balance <A>          - show balance\n"
            "  min <A>              - show min balance\n"
            "  max <A>              - show max balance\n"
            "  freeze <A>           - freeze account\n"
            "  unfreeze <A>         - unfreeze account\n"
            "  transfer <A> <B> <X> - transfer X from A to B\n"
            "  deposit <X>          - deposit X to all\n"
            "  withdraw <X>         - withdraw X from all\n"
            "  setmin <A> <X>       - set min balance\n"
            "  setmax <A> <X>       - set max balance\n"
            "  exit                 - quit");
    } else {
        snprintf(msg, sz, "Error: unknown command '%s'. Type 'help' for commands.", op.c_str());
    }
}
