#!/usr/bin/env bash

set -u

N="${1:-10}"

RESTORE_SCRIPT="./restore_kvm.sh"
FUZZ_SCRIPT="./fuzz_kvm.sh"
DELAY=2

for ((i = 1; i <= N; i++)); do
    echo "========================================"
    echo "Iterazione $i/$N"
    echo "========================================"

    # 1. Avvia il restore in background.
    "$RESTORE_SCRIPT" &
    restore_pid=$!

    echo "[+] Restore avviato (PID=$restore_pid)"

    # 2. Attendi due secondi prima di avviare il fuzz.
    sleep "$DELAY"

    # Se il restore è già terminato, non avviare il fuzz.
    if ! kill -0 "$restore_pid" 2>/dev/null; then
        wait "$restore_pid"
        restore_status=$?

        echo "[!] Restore terminato prima dell'avvio del fuzz" \
             "(exit=$restore_status)"
        continue
    fi

    # 3. Avvia il fuzz in background.
    "$FUZZ_SCRIPT" &
    fuzz_pid=$!

    echo "[+] Fuzz avviato (PID=$fuzz_pid)"

    # 4. Attendi la conclusione del restore, non del fuzz.
    wait "$restore_pid"
    restore_status=$?

    echo "[+] Restore terminato (exit=$restore_status)"

    # 5. Evita che il fuzz della vecchia iterazione
    #    continui durante la successiva.
    if kill -0 "$fuzz_pid" 2>/dev/null; then
        echo "[+] Arresto del fuzz (PID=$fuzz_pid)"
        kill -TERM "$fuzz_pid" 2>/dev/null || true
    fi

    # Raccoglie il risultato del processo fuzz.
    wait "$fuzz_pid" 2>/dev/null || true

    echo "[+] Iterazione $i completata"
done

echo "Campagna terminata: $N iterazioni."
