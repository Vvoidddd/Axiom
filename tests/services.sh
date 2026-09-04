#!/usr/bin/env bash
set -euo pipefail
iso=${1:-axiom.iso};qemu=${QEMU:-qemu-system-x86_64};ovmf=${OVMF:-};out=${2:-build/services}
keys(){ local text=$1 char;for((i=0;i<${#text};i++));do char=${text:i:1};case $char in ' ')char=spc;; '-')char=minus;; '/')char=slash;; esac;printf 'sendkey %s\n' "$char";sleep .04;done;echo sendkey ret;sleep .15;}
run_case(){ local firmware=$1 wait_time=18 log="$out-$1.log";local -a fw=();[[ $firmware == uefi ]]&&{ wait_time=21;fw=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf");};rm -f "$log" "$out-$firmware.ppm";{
 sleep "$wait_time";keys axiomadmin;sleep 1;keys axiomtest123;sleep 1;keys axiomtest123;sleep 1;keys '';sleep 1;keys axiomadmin;sleep 1;keys axiomtest123;sleep 3
 keys 'service restart system-logger --confirm';sleep 1
 keys 'elevate service axiomtest123';sleep 1;keys 'service restart system-logger --confirm';sleep 2;keys servicetest;sleep 1
 echo "screendump $out-$firmware.ppm";keys 'elevate power axiomtest123';sleep 1;keys 'reboot --confirm';sleep 2
 }|timeout 60 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1||true
 grep -q '\[AUDIT\] permission-denied' "$log";grep -q '\[AUDIT\] service-control' "$log";grep -q '\[PASS\] USER-SPACE SERVICE SUPERVISION TEST' "$log";[[ $(grep -c '\[SERVICE-SPAWN\]' "$log") -eq 3 ]];! grep -q '\[ERROR\] CPU exception' "$log";test -s "$out-$firmware.ppm";echo "PASS: $firmware supervised service lifecycle";}
command -v "$qemu" >/dev/null;[[ -n $ovmf&&-f $ovmf ]];mkdir -p "$(dirname "$out")";run_case bios;run_case uefi;echo 'All Axiom service tests passed.'
