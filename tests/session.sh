#!/usr/bin/env bash
set -euo pipefail

iso=${1:-axiom.iso}
qemu=${QEMU:-qemu-system-x86_64}
ovmf=${OVMF:-}
out=${2:-build/session}

keys() {
    local text=$1 char
    for ((i=0; i<${#text}; i++)); do
        char=${text:i:1}
        case $char in ' ') char=spc;; '-') char=minus;; '/') char=slash;; esac
        printf 'sendkey %s\n' "$char"
        sleep .04
    done
    echo 'sendkey ret'
    sleep .15
}

run_session_case() {
    local firmware=$1 wait_time=18
    local log="$out-$firmware.log"
    local -a firmware_args=()
    [[ $firmware == uefi ]] && {
        wait_time=21
        firmware_args=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf")
    }
    rm -f "$log" "$out-$firmware-terminal.ppm" "$out-$firmware-locked.ppm" "$out-$firmware-logout.ppm"
    {
        sleep "$wait_time"
        keys axiomadmin; sleep 1
        keys axiomtest123; sleep 1
        keys axiomtest123; sleep 1
        keys ''; sleep 1
        keys axiomadmin; sleep 1
        keys axiomtest123; sleep 3
        keys id; sleep 1
        echo "screendump $out-$firmware-terminal.ppm"
        keys lock; sleep 2
        echo "screendump $out-$firmware-locked.ppm"
        keys axiomadmin; sleep 1
        keys axiomtest123; sleep 3
        keys logout; sleep 2
        echo "screendump $out-$firmware-logout.ppm"
        echo quit
    } | timeout 55 "$qemu" -M q35 -smp 4 -m 512M "${firmware_args[@]}" \
        -cdrom "$iso" -boot d -display none -serial "file:$log" \
        -monitor stdio -no-reboot >/dev/null 2>&1 || true

    grep -q '\[CMD\] id' "$log"
    grep -q 'session processes inherit authenticated credentials' "$log"
    grep -q 'session locked; returning to login' "$log"
    grep -q 'session logged out; returning to login' "$log"
    [[ $(grep -c 'graphical login successful' "$log") -ge 2 ]]
    ! grep -q '\[ERROR\] CPU exception' "$log"
    test -s "$out-$firmware-terminal.ppm"
    test -s "$out-$firmware-locked.ppm"
    test -s "$out-$firmware-logout.ppm"
    echo "PASS: $firmware authenticated session lifecycle"
}

command -v "$qemu" >/dev/null
[[ -n $ovmf && -f $ovmf ]] || { echo "Set OVMF to an OVMF_CODE firmware file" >&2; exit 1; }
mkdir -p "$(dirname "$out")"
run_session_case bios
run_session_case uefi
echo 'All Axiom session lifecycle tests passed.'
