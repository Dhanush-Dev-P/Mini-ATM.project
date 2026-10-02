#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ATTEMPTS   3
#define MAX_HISTORY    10
#define MAX_ACCOUNTS   50
#define DATA_FILE      "atm_data.dat"
#define UTC_OFFSET_MINUTES 330   /* India = +5:30. Change for another zone. */

/* One transaction record */
struct Transaction {
    char   type[20];      /* "Opening", "Deposit", "Withdraw", "Transfer Out", "Transfer In" */
    double amount;
    double balanceAfter;
    int    otherAcc;       /* counterparty account number for transfers, else 0 */
    time_t timestamp;
};

struct History {
    struct Transaction items[MAX_HISTORY];
    int count;
};

struct Account {
    int accountNumber;
    int pin;
    double balance;
    struct History history;
};

/* Everything saved to the file: every account, plus how many exist */
struct Bank {
    struct Account accounts[MAX_ACCOUNTS];
    int count;
};

/* Function prototypes */
int  loadBank(struct Bank *bank);
void saveBank(const struct Bank *bank);
int  findAccount(struct Bank *bank, int accNo);
int  createAccount(struct Bank *bank);
int  login(struct Bank *bank);

void showMenu(void);
void checkBalance(const struct Account *acc);
void deposit(struct Account *acc);
void withdraw(struct Account *acc);
void transfer(struct Bank *bank, int myIndex);
void showHistory(const struct Account *acc);
void addTransaction(struct History *history, const char *type,
                    double amount, double balanceAfter, int otherAcc);
void formatTime(time_t t, char *buffer, size_t size);
void changePin(struct Account *acc);
void clearInputBuffer(void);

int main(void) {
    struct Bank bank;
    int myIndex;
    int choice;

    if (!loadBank(&bank)) {
        bank.count = 0;   /* no file yet, start empty */
    }

    /* Top-level menu: log in or create a new account */
    while (1) {
        printf("\n    Wellcome To Bank\n");
        printf("1. Login\n");
        printf("2. Create New Account\n");
        printf("Enter choice: ");
        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            continue;
        }
        if (choice == 2) {
            createAccount(&bank);
            saveBank(&bank);
        } else if (choice == 1) {
            myIndex = login(&bank);
            if (myIndex >= 0)
                break;   /* logged in, go to main menu */
            printf("Too many wrong attempts. transaction cancelled(MAX_ATTEMPTS).\n");
        } else {
            printf("Invalid choice. Try again.\n");
        }
    }

    do {
        showMenu();
        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            choice = 0;
        }

        switch (choice) {
            case 1: checkBalance(&bank.accounts[myIndex]);        break;
            case 2: deposit(&bank.accounts[myIndex]);             break;
            case 3: withdraw(&bank.accounts[myIndex]);            break;
            case 4: transfer(&bank, myIndex);                     break;
            case 5: showHistory(&bank.accounts[myIndex]);         break;
            case 6: changePin(&bank.accounts[myIndex]);           break;
            case 7: printf("Thank you. Goodbye!\n");              break;
            default: printf("Invalid choice. Try again.\n");
        }

        saveBank(&bank);   /* save after every action */
    } while (choice != 7);

    return 0;
}

/* ---------- Account creation & login ---------- */

int createAccount(struct Bank *bank) {
    struct Account *acc;

    if (bank->count >= MAX_ACCOUNTS) {
        printf("Bank is full. Cannot create more accounts.\n");
        return -1;
    }

    acc = &bank->accounts[bank->count];
    acc->accountNumber = 100001 + bank->count;   /* auto-assigned */
    acc->history.count = 0;

    printf("=== Create Your Account ===\n");
    printf("Your account number is: %d (remember this to log in)\n", acc->accountNumber);

    while (1) {
        printf("Set a 4-digit PIN: ");
        if (scanf("%d", &acc->pin) == 1 && acc->pin >= 1000 && acc->pin <= 9999)
            break;
        printf("Invalid PIN. Enter exactly 4 digits (1000-9999).\n");
        clearInputBuffer();
    }

    while (1) {
        printf("Enter opening balance: ");
        if (scanf("%lf", &acc->balance) == 1 && acc->balance >= 0)
            break;
        printf("Invalid amount. Enter 0 or a positive number.\n");
        clearInputBuffer();
    }

    addTransaction(&acc->history, "Opening", acc->balance, acc->balance, 0);
    bank->count++;
    printf("Account created and saved.\n");
    return acc->accountNumber;
}

int findAccount(struct Bank *bank, int accNo) {
    int i;
    for (i = 0; i < bank->count; i++)
        if (bank->accounts[i].accountNumber == accNo)
            return i;
    return -1;
}

/* Returns the index of the logged-in account, or -1 on failure */
int login(struct Bank *bank) {
    int accNo, idx, enteredPin, attempts;

    printf("Enter account number: ");
    if (scanf("%d", &accNo) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        return -1;
    }

    idx = findAccount(bank, accNo);
    if (idx == -1) {
        printf("No account with that number.\n");
        return -1;
    }

    for (attempts = 1; attempts <= MAX_ATTEMPTS; attempts++) {
        printf("Enter PIN (attempt %d of %d): ", attempts, MAX_ATTEMPTS);
        if (scanf("%d", &enteredPin) != 1) {
            clearInputBuffer();
            printf("Invalid input.\n");
            continue;
        }
        if (enteredPin == bank->accounts[idx].pin) {
            printf("Login successful!\n");
            return idx;
        }
        printf("Wrong PIN,Try again.\n");
    }
    return -1;
}

/* ---------- Menu actions ---------- */

void showMenu(void) {
    printf("\n===== ATM MENU =====\n");
    printf("1. Check Balance\n");
    printf("2. Deposit\n");
    printf("3. Withdraw\n");
    printf("4. Transfer to Another Account\n");
    printf("5. Transaction History\n");
    printf("6. Change PIN\n");
    printf("7. Exit\n");
    printf("Enter choice: ");
}

void checkBalance(const struct Account *acc) {
    printf("Your current balance is: %.2f\n", acc->balance);
}

void deposit(struct Account *acc) {
    double amount;
    char when[50];

    printf("Enter amount to deposit: ");
    if (scanf("%lf", &amount) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        return;
    }
    if (amount <= 0) {
        printf("Deposit amount must be greater than 0.\n");
        return;
    }

    acc->balance += amount;
    addTransaction(&acc->history, "Deposit", amount, acc->balance, 0);
    formatTime(acc->history.items[acc->history.count - 1].timestamp, when, sizeof(when));
    printf("\n--- Deposit Successful ---\n");
    printf("Amount credited : %.2f\n", amount);
    printf("Total balance   : %.2f\n", acc->balance);
    printf("Date & Time     : %s\n", when);
}

void withdraw(struct Account *acc) {
    double amount;
    char when[50];

    printf("Enter amount to withdraw: ");
    if (scanf("%lf", &amount) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        return;
    }
    if (amount <= 0) {
        printf("Withdrawal amount must be greater than 0.\n");
        return;
    }
    if (amount > acc->balance) {
        printf("Insufficient balance.\n");
        return;
    }

    acc->balance -= amount;
    addTransaction(&acc->history, "Withdraw", amount, acc->balance, 0);
    formatTime(acc->history.items[acc->history.count - 1].timestamp, when, sizeof(when));
    printf("\n--- Withdrawal Successful ---\n");
    printf("Amount debited  : %.2f\n", amount);
    printf("Total balance   : %.2f\n", acc->balance);
    printf("Date & Time     : %s\n", when);
}

/* Moves money from the logged-in account to another account in the same bank */
void transfer(struct Bank *bank, int myIndex) {
    int toAccNo, toIndex;
    double amount;
    char when[50];
    struct Account *from = &bank->accounts[myIndex];
    struct Account *to;

    printf("Enter recipient account number: ");
    if (scanf("%d", &toAccNo) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        return;
    }

    if (toAccNo == from->accountNumber) {
        printf("You cannot transfer to your own account.\n");
        return;
    }

    toIndex = findAccount(bank, toAccNo);
    if (toIndex == -1) {
        printf("No account with that number.\n");
        return;
    }
    to = &bank->accounts[toIndex];

    printf("Enter amount to transfer: ");
    if (scanf("%lf", &amount) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        return;
    }
    if (amount <= 0) {
        printf("Transfer amount must be greater than 0.\n");
        return;
    }
    if (amount > from->balance) {
        printf("Insufficient balance.\n");
        return;
    }

    from->balance -= amount;
    to->balance   += amount;

    addTransaction(&from->history, "Transfer Out", amount, from->balance, to->accountNumber);
    addTransaction(&to->history,   "Transfer In",  amount, to->balance,   from->accountNumber);

    formatTime(from->history.items[from->history.count - 1].timestamp, when, sizeof(when));
    printf("\n--- Transfer Successful ---\n");
    printf("Amount sent      : %.2f\n", amount);
    printf("To account       : %d\n", to->accountNumber);
    printf("Your new balance : %.2f\n", from->balance);
    printf("Date & Time      : %s\n", when);
}

/* Saves a transaction with the current date/time; oldest is dropped when full */
void addTransaction(struct History *history, const char *type,
                    double amount, double balanceAfter, int otherAcc) {
    int i;
    struct Transaction *t;

    if (history->count == MAX_HISTORY) {
        for (i = 1; i < MAX_HISTORY; i++)
            history->items[i - 1] = history->items[i];
        history->count--;
    }

    t = &history->items[history->count];
    strncpy(t->type, type, sizeof(t->type) - 1);
    t->type[sizeof(t->type) - 1] = '\0';
    t->amount = amount;
    t->balanceAfter = balanceAfter;
    t->otherAcc = otherAcc;
    t->timestamp = time(NULL);
    history->count++;
}

/* Uses UTC plus a fixed offset, so it does not depend on the compiler's time zone */
void formatTime(time_t t, char *buffer, size_t size) {
    struct tm *info;
    t += (time_t)UTC_OFFSET_MINUTES * 60;
    info = gmtime(&t);
    strftime(buffer, size, "[%d-%m-%Y] [%H:%M:%S]", info);
}

void showHistory(const struct Account *acc) {
    int i;
    char when[50];
    const struct History *history = &acc->history;

    printf("\n--- Transaction History (last %d) ---\n", MAX_HISTORY);
    if (history->count == 0) {
        printf("No transactions yet.\n");
        return;
    }

    printf("%-4s %-20s %-13s %10s %12s %10s\n",
           "No.", "Date & Time", "Type", "Amount", "Balance", "With Acc");
    for (i = 0; i < history->count; i++) {
        formatTime(history->items[i].timestamp, when, sizeof(when));
        if (history->items[i].otherAcc != 0)
            printf("%-4d %-20s %-13s %10.2f %12.2f %10d\n", i + 1, when,
                   history->items[i].type, history->items[i].amount,
                   history->items[i].balanceAfter, history->items[i].otherAcc);
        else
            printf("%-4d %-20s %-13s %10.2f %12.2f %10s\n", i + 1, when,
                   history->items[i].type, history->items[i].amount,
                   history->items[i].balanceAfter, "-");
    }
}

void changePin(struct Account *acc) {
    int oldPin, newPin, confirmPin;

    while (1) {
        printf("Enter current PIN (0 to cancel): ");
        if (scanf("%d", &oldPin) != 1) {
            clearInputBuffer();
            printf("Invalid input.\n");
            continue;
        }
        if (oldPin == 0) {
            printf("PIN change cancelled.\n");
            return;
        }
        if (oldPin == acc->pin)
            break;
        printf("Wrong PIN. Try again.\n");
    }

    while (1) {
        printf("Enter new 4-digit PIN (0 to cancel): ");
        if (scanf("%d", &newPin) != 1) {
            clearInputBuffer();
            printf("Invalid input.\n");
            continue;
        }
        if (newPin == 0) {
            printf("PIN change cancelled.\n");
            return;
        }
        if (newPin >= 1000 && newPin <= 9999)
            break;
        printf("PIN must be exactly 4 digits.\n");
    }

    while (1) {
        printf("Confirm new PIN (0 to cancel): ");
        if (scanf("%d", &confirmPin) != 1) {
            clearInputBuffer();
            printf("Invalid input.\n");
            continue;
        }
        if (confirmPin == 0) {
            printf("PIN change cancelled.\n");
            return;
        }
        if (confirmPin == newPin)
            break;
        printf("PINs do not match. Re-enter the confirm PIN.\n");
    }

    acc->pin = newPin;
    printf("PIN changed successfully.\n");
}

/* ---------- File I/O for the whole bank ---------- */

int loadBank(struct Bank *bank) {
    FILE *fp = fopen(DATA_FILE, "rb");
    size_t n;

    if (fp == NULL)
        return 0;

    n = fread(bank, sizeof(struct Bank), 1, fp);
    fclose(fp);

    if (n != 1 || bank->count < 0 || bank->count > MAX_ACCOUNTS)
        return 0;
    return 1;
}

void saveBank(const struct Bank *bank) {
    FILE *fp = fopen(DATA_FILE, "wb");
    if (fp == NULL) {
        printf("Warning: could not save data to file.\n");
        return;
    }
    fwrite(bank, sizeof(struct Bank), 1, fp);
    fclose(fp);
}

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}
