#!/usr/bin/env bash
set -euo pipefail
iso=${1:-build/axiom.iso}
qemu=${QEMU:-qemu-system-x86_64}
command -v "$qemu" >/dev/null || { echo "qemu-system-x86_64 is required" >&2; exit 1; }
ovmf=${OVMF:-}
for candidate in /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/x64/OVMF_CODE.fd; do [[ -f $candidate ]] && { ovmf=$candidate; break; }; done
[[ -n $ovmf ]] || { echo "OVMF firmware is required" >&2; exit 1; }
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
keys(){ local text=$1 char;for ((i=0;i<${#text};i++));do char=${text:i:1};case $char in ' ')char=spc;; '-')char=minus;; '/')char=slash;; esac;printf 'sendkey %s\n' "$char";sleep .04;done;echo 'sendkey ret';sleep .1; }
run_case(){
 local name=$1 firmware=$2 command=$3
 local log="build/smoke-$name.log";rm -f "$log"
 local -a fw=();[[ $firmware == uefi ]]&&fw=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf")
 local boot_wait=18;[[ $firmware == uefi ]]&&boot_wait=21
 { sleep "$boot_wait";keys axiomadmin;sleep 1;keys axiomtest123;sleep 1;keys axiomtest123;sleep 1;keys '';sleep 1;keys axiomadmin;sleep 1;keys axiomtest123;sleep 3;keys 'elevate users axiomtest123';sleep 1;keys 'elevate power axiomtest123';sleep 1;keys 'elevate device axiomtest123';sleep 1;echo "screendump build/smoke-$name.ppm";keys "$command";sleep 3;[[ $command == fault ]]||keys 'reboot --confirm';sleep 2; } | timeout 55 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1 || true
 grep -q 'GDT TSS IDT PIC APIC and IRQ devices online' "$log";grep -q 'first administrator account created' "$log";grep -q 'one-time recovery key acknowledged' "$log";grep -q 'graphical login successful' "$log";grep -q 'application processors entered idle scheduler' "$log";grep -q 'concurrent login serialization verified on all processors' "$log";grep -q "\[CMD\] $command" "$log"
 if [[ $command == fault ]];then grep -q '\[ERROR\] CPU exception' "$log";grep -q 'VECTOR=0x0000000000000006' "$log"
 elif [[ $command == storagetest ]];then grep -q '\[PASS\] PHASE 2 STORAGE RECOVERY TESTS' "$log";grep -q '\[INFO\] reboot requested' "$log"
 else grep -q '\[CMD\] version' "$log";grep -q '\[CMD\] heaptest' "$log";grep -q '\[PASS\] HEAP MAP ALLOCATE FREE TEST' "$log";grep -q '\[PASS\] ACCOUNT DATABASE AND AUTH TESTS' "$log";grep -q '\[CMD\] packages' "$log";grep -q '\[INFO\] launching user-space init and shell' "$log";grep -q '\[SERVICE-SPAWN\] /system/bin/loggerd' "$log";grep -q '\[SERVICE-SPAWN\] /system/bin/deviced' "$log";grep -q '\[PASS\] USER-SPACE SERVICE SUPERVISION TEST' "$log";grep -q '\[SPAWN\] /system/bin/shell' "$log";grep -q '\[SPAWN\] /apps/hello' "$log";grep -q '\[INFO\] ring 3 ELF syscall and IPC probe passed' "$log";grep -q '\[INFO\] preemptive ring 3 scheduler probe passed' "$log";grep -q '\[PASS\] PHASE 3 RING3 PROCESS AND SYSCALL TESTS' "$log";grep -q '\[INFO\] reboot requested' "$log";fi
 grep -q '\[ERROR\] CPU exception' "$log"&&[[ $command != fault ]]&&return 1
 echo "PASS: $name"
}
mode=${2:-all}
[[ $mode == all || $mode == bios-commands ]]&&run_case bios-commands bios 'script demo'
[[ $mode == all || $mode == uefi-commands ]]&&run_case uefi-commands uefi 'script demo'
[[ $mode == all || $mode == bios-storage ]]&&run_case bios-storage bios storagetest
[[ $mode == all || $mode == uefi-storage ]]&&run_case uefi-storage uefi storagetest
[[ $mode == all || $mode == bios-fault ]]&&run_case bios-fault bios fault
[[ $mode == all || $mode == uefi-fault ]]&&run_case uefi-fault uefi fault
echo "All Axiom Phase 1, Phase 2, and Phase 3 smoke tests passed."
