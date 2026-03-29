#!/bin/bash

PASS=0
FAIL=0
DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR"

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -q "$expected"; then
        echo "  PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "  FAIL: $desc"
        echo "    Expected: $expected"
        echo "    Got: $actual"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Basic Tests ==="

# cleanup
./bank_deinit 2>/dev/null || true

# init
echo "--- Init/Deinit ---"
OUT=$(./bank_init 5)
check "bank_init creates bank" "Bank created with 5 accounts" "$OUT"

# basic balance
echo "--- Balance queries ---"
OUT=$(echo "balance 0" | ./client_local 2>/dev/null)
check "initial balance is 0" "Balance of account 0: 0" "$OUT"

OUT=$(echo "min 0" | ./client_local 2>/dev/null)
check "initial min is 0" "Min balance of account 0: 0" "$OUT"

OUT=$(echo "max 0" | ./client_local 2>/dev/null)
check "initial max" "Max balance of account 0: 1000000" "$OUT"

# invalid account
echo "--- Error handling ---"
OUT=$(echo "balance 99" | ./client_local 2>/dev/null)
check "invalid account" "Error: invalid account 99" "$OUT"

OUT=$(echo "balance -1" | ./client_local 2>/dev/null)
check "negative account" "Error: invalid account" "$OUT"

OUT=$(echo "blahblah" | ./client_local 2>/dev/null)
check "unknown command" "Error: unknown command" "$OUT"

# deposit
echo "--- Deposit/Withdraw ---"
OUT=$(echo -e "deposit 100\nbalance 0\nbalance 1" | ./client_local 2>/dev/null)
check "deposit to all" "Deposited 100 to all" "$OUT"
check "balance after deposit acc 0" "Balance of account 0: 100" "$OUT"
check "balance after deposit acc 1" "Balance of account 1: 100" "$OUT"

# withdraw
OUT=$(echo -e "withdraw 50\nbalance 0" | ./client_local 2>/dev/null)
check "withdraw from all" "Withdrew 50 from all" "$OUT"
check "balance after withdraw" "Balance of account 0: 50" "$OUT"

# transfer
echo "--- Transfer ---"
OUT=$(echo -e "transfer 0 1 30\nbalance 0\nbalance 1" | ./client_local 2>/dev/null)
check "transfer success" "Transferred 30 from account 0 to account 1" "$OUT"
check "sender balance" "Balance of account 0: 20" "$OUT"
check "receiver balance" "Balance of account 1: 80" "$OUT"

# transfer to same account
OUT=$(echo "transfer 0 0 10" | ./client_local 2>/dev/null)
check "transfer to self" "Error: cannot transfer to the same account" "$OUT"

# transfer negative amount
OUT=$(echo "transfer 0 1 -5" | ./client_local 2>/dev/null)
check "transfer negative" "Error: transfer amount must be positive" "$OUT"

# freeze
echo "--- Freeze/Unfreeze ---"
OUT=$(echo -e "freeze 0\ntransfer 0 1 10" | ./client_local 2>/dev/null)
check "freeze account" "Account 0 frozen successfully" "$OUT"
check "transfer from frozen" "Error: account 0 is frozen" "$OUT"

OUT=$(echo -e "unfreeze 0\ntransfer 0 1 10" | ./client_local 2>/dev/null)
check "unfreeze account" "Account 0 unfrozen successfully" "$OUT"
check "transfer after unfreeze" "Transferred 10" "$OUT"

# setmin/setmax
echo "--- Set min/max ---"
OUT=$(echo -e "setmin 0 -100\nmin 0" | ./client_local 2>/dev/null)
check "set min balance" "Min balance of account 0 set to -100" "$OUT"
check "verify min" "Min balance of account 0: -100" "$OUT"

OUT=$(echo -e "setmax 0 500\nmax 0" | ./client_local 2>/dev/null)
check "set max balance" "Max balance of account 0 set to 500" "$OUT"
check "verify max" "Max balance of account 0: 500" "$OUT"

# exceed limits
OUT=$(echo "deposit 1000" | ./client_local 2>/dev/null)
check "deposit exceeds max" "Error.*exceed max" "$OUT"

# cleanup
./bank_deinit

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ $FAIL -eq 0 ] && exit 0 || exit 1
