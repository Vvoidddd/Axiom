#!/usr/bin/env bash
set -euo pipefail
iso=${1:-build/axiom.iso}
qemu=${QEMU:-qemu-system-x86_64}
command -v "$qemu" >/dev/null || { echo "qemu-system-x86_64 is required" >&2; exit 1; }
ovmf=${OVMF:-}
for candidate in /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/x64/OVMF_CODE.fd; do [[ -f $candidate ]] && { ovmf=$candidate; break; }; done
[[ -n $ovmf ]] || { echo "OVMF firmware is required" >&2; exit 1; }
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
keys(){ local text=$1 char;for ((i=0;i<${#text};i++));do char=${text:i:1};[[ $char == " " ]]&&char=spc;printf 'sendkey %s\n' "$char";done;echo 'sendkey ret'; }
run_case(){
 local name=$1 firmware=$2 command=$3
 local log="$tmp/$name.log"
 local -a fw=();[[ $firmware == uefi ]]&&fw=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf")
 local boot_wait=15;[[ $firmware == uefi ]]&&boot_wait=18
 { sleep "$boot_wait";keys "$command";sleep 3;[[ $command == fault ]]||keys reboot;sleep 2; } | timeout 30 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1 || true
 grep -q 'GDT TSS IDT PIC APIC and IRQ devices online' "$log";grep -q 'application processors entered idle scheduler' "$log";grep -q "\[CMD\] $command" "$log"
 if [[ $command == fault ]];then grep -q '\[ERROR\] CPU exception' "$log";grep -q 'VECTOR=0x0000000000000006' "$log"
 elif [[ $command == storagetest ]];then grep -q '\[PASS\] PHASE 2 STORAGE RECOVERY TESTS' "$log";grep -q '\[INFO\] reboot requested' "$log"
 else grep -q '\[CMD\] version' "$log";grep -q '\[CMD\] heaptest' "$log";grep -q '\[PASS\] HEAP MAP ALLOCATE FREE TEST' "$log";grep -q '\[PASS\] ACCOUNT DATABASE AND AUTH TESTS' "$log";grep -q '\[CMD\] packages' "$log";grep -q '\[INFO\] launching user-space init and shell' "$log";grep -q '\[SPAWN\] /system/bin/shell' "$log";grep -q '\[SPAWN\] /apps/hello' "$log";grep -q '\[INFO\] ring 3 ELF syscall and IPC probe passed' "$log";grep -q '\[INFO\] preemptive ring 3 scheduler probe passed' "$log";grep -q '\[PASS\] PHASE 3 RING3 PROCESS AND SYSCALL TESTS' "$log";grep -q '\[INFO\] reboot requested' "$log";fi
 grep -q '\[ERROR\] CPU exception' "$log"&&[[ $command != fault ]]&&return 1
 echo "PASS: $name"
}
run_case bios-commands bios 'script demo'
run_case uefi-commands uefi 'script demo'
run_case bios-storage bios storagetest
run_case uefi-storage uefi storagetest
run_case bios-fault bios fault
run_case uefi-fault uefi fault
echo "All Axiom Phase 1, Phase 2, and Phase 3 smoke tests passed."
