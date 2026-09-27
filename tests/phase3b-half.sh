#!/usr/bin/env bash
set -euo pipefail

iso=${1:-axiom.iso}
qemu=${QEMU:-qemu-system-x86_64}
ovmf=${OVMF:-}
out=${2:-build/phase3b-half}

keys() {
    local text=$1 char
    for ((i=0; i<${#text}; i++)); do
        char=${text:i:1}
        case $char in
            ' ') char=spc ;;
            '-') char=minus ;;
            '/') char=slash ;;
            '.') char=dot ;;
        esac
        printf 'sendkey %s\n' "$char"
        sleep .035
    done
    echo 'sendkey ret'
    sleep .15
}

run_case() {
    local firmware=$1 wait_time=18 log
    log="$out-$firmware.log"
    local -a firmware_args=()
    if [[ $firmware == uefi ]]; then
        wait_time=21
        firmware_args=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf")
    fi
    rm -f "$log" "$out-$firmware.ppm"
    {
        sleep "$wait_time"
        keys axiomadmin
        sleep 1
        keys axiomtest123
        sleep 1
        keys axiomtest123
        sleep 1
        keys ''
        sleep 1
        keys axiomadmin
        sleep 1
        keys axiomtest123
        sleep 3
        keys 'elevate service axiomtest123'
        sleep 1
        keys 'elevate device axiomtest123'
        sleep 1
        keys 'elevate audit axiomtest123'
        sleep 1
        keys 'elevate power axiomtest123'
        sleep 1
        keys configtest
        sleep 1
        keys devicetest
        sleep 1
        keys 'setconfig system hostname axiom-lab --confirm'
        sleep 1
        keys 'config system hostname'
        sleep 1
        keys 'setconfig user theme light'
        sleep 1
        keys 'config user theme'
        sleep 1
        keys devices
        sleep 1
        keys deviceevents
        sleep 1
        keys servicetest
        sleep 1
        keys journal
        sleep 1
        keys settings
        sleep 1
        keys powerstatus
        sleep 1
        echo "screendump $out-$firmware.ppm"
        keys 'reboot --confirm'
        sleep 3
    } | timeout 80 "$qemu" -M q35 -smp 4 -m 512M "${firmware_args[@]}" \
        -cdrom "$iso" -boot d -display none -serial "file:$log" \
        -monitor stdio -no-reboot >/dev/null 2>&1 || true

    grep -q '\[INFO\] persistent system journal online' "$log"
    grep -q '\[INFO\] stable device namespace and hotplug event queue online' "$log"
    grep -q '\[PASS\] CONFIG SCHEMA MIGRATION AND RECOVERY TEST' "$log"
    grep -q '\[PASS\] DEVICE DISCOVERY PERMISSIONS HOTPLUG TEST' "$log"
    grep -q '\[PASS\] USER-SPACE SERVICE SUPERVISION TEST' "$log"
    grep -q '\[AUDIT\] configuration-change' "$log"
    grep -q '\[CMD\] config system hostname' "$log"
    grep -q '\[CMD\] config user theme' "$log"
    grep -q '\[CMD\] deviceevents' "$log"
    grep -q '\[CMD\] journal' "$log"
    grep -q '\[CMD\] powerstatus' "$log"
    grep -q '\[POWER\] all application and storage writes flushed' "$log"
    grep -q '\[SERVICE\] stopped system-logger' "$log"
    grep -q '\[SERVICE\] stopped device-manager' "$log"
    grep -q '\[INFO\] reboot requested' "$log"
    ! grep -q '\[ERROR\]' "$log"
    test -s "$out-$firmware.ppm"
    echo "PASS: $firmware Phase 3B core services"
}

command -v "$qemu" >/dev/null
[[ -n $ovmf && -f $ovmf ]]
run_case bios
run_case uefi
echo 'All implemented Axiom Phase 3B core-service tests passed.'
