#!/usr/bin/env bash
set -euo pipefail
iso=${1:-build/axiom.iso};qemu=${QEMU:-qemu-system-x86_64}
ovmf=${OVMF:-};for f in /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/x64/OVMF_CODE.fd;do [[ -f $f ]]&&{ ovmf=$f;break;};done
command -v "$qemu" >/dev/null;command -v truncate >/dev/null;[[ -n $ovmf ]]
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
truncate -s 32M "$tmp/sata.img";truncate -s 32M "$tmp/nvme.img"
keys(){ local text=$1 char;for((i=0;i<${#text};i++));do char=${text:i:1};case $char in ' ')char=spc;; '-')char=minus;; '/')char=slash;; esac;printf 'sendkey %s\n' "$char";sleep .035;done;echo 'sendkey ret';sleep .15;}
run(){
 local controller=$1 firmware=$2 log="build/hardware-storage-$1-$2.log"
 local -a fw=() disk=()
 [[ $firmware == uefi ]]&&fw=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf")
 if [[ $controller == ahci ]];then
  disk=(-drive "if=none,id=satadisk,format=raw,file=$tmp/sata.img" -device ide-hd,drive=satadisk,bus=ide.0)
 else
  disk=(-drive "if=none,id=nvmedisk,format=raw,file=$tmp/nvme.img" -device nvme,drive=nvmedisk,serial=AXIOMNVME)
 fi
 rm -f "$log"
 local boot_wait=20;[[ $firmware == uefi ]]&&boot_wait=23
 { sleep "$boot_wait";keys axiomadmin;sleep 1;keys axiomtest123;sleep 1;keys axiomtest123;sleep 1;keys '';sleep 1;keys axiomadmin;sleep 1;keys axiomtest123;sleep 3;keys 'elevate device axiomtest123';sleep 1;keys 'elevate power axiomtest123';sleep 1;keys hwstoragetest;sleep 3;keys 'reboot --confirm';sleep 2;} | timeout 65 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d "${disk[@]}" -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1||true
 grep -q '\[PASS\] HARDWARE STORAGE DEVICE ATTACHED AND READ ONLY' "$log"
 echo "PASS: $firmware $controller read-only device"
}
mode=${2:-all}
[[ $mode == all || $mode == ahci ]]&&run ahci bios
[[ $mode == all || $mode == ahci ]]&&run ahci uefi
# SeaBIOS attempts to initialize an attached NVMe device before Limine and
# stalls this QEMU fixture. The same driver is exercised under OVMF here.
[[ $mode == all || $mode == nvme ]]&&run nvme uefi
