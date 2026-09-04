#!/usr/bin/env bash
set -euo pipefail
iso=${1:-build/axiom.iso};qemu=${QEMU:-qemu-system-x86_64}
ovmf=${OVMF:-};for f in /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/x64/OVMF_CODE.fd;do [[ -f $f ]]&&{ ovmf=$f;break;};done
command -v "$qemu" >/dev/null;command -v qemu-img >/dev/null;[[ -n $ovmf ]]
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
qemu-img create -q -f raw "$tmp/sata.img" 32M;qemu-img create -q -f raw "$tmp/nvme.img" 32M
keys(){ local text=$1 char;for((i=0;i<${#text};i++));do char=${text:i:1};[[ $char == " " ]]&&char=spc;printf 'sendkey %s\n' "$char";done;echo 'sendkey ret';}
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
 { sleep 25;keys hwstoragetest;sleep 3;keys reboot;sleep 2;} | timeout 40 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d "${disk[@]}" -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1||true
 grep -q '\[PASS\] HARDWARE STORAGE DEVICE ATTACHED AND READ ONLY' "$log"
 echo "PASS: $firmware $controller read-only device"
}
mode=${2:-all}
[[ $mode == all || $mode == ahci ]]&&run ahci bios
[[ $mode == all || $mode == ahci ]]&&run ahci uefi
# SeaBIOS attempts to initialize an attached NVMe device before Limine and
# stalls this QEMU fixture. The same driver is exercised under OVMF here.
[[ $mode == all || $mode == nvme ]]&&run nvme uefi
