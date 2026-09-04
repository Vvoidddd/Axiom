#!/usr/bin/env bash
set -euo pipefail
iso=${1:-build/axiom.iso};qemu=${QEMU:-qemu-system-x86_64};out=${2:-build}
keys(){ local text=$1 char;for((i=0;i<${#text};i++));do char=${text:i:1};case $char in ' ')char=spc;; '-')char=minus;; '/')char=slash;; esac;printf 'sendkey %s\n' "$char";sleep .04;done;echo sendkey ret;sleep .1;}
mkdir -p "$out";rm -f "$out"/visual-*.ppm "$out/visual-login.log"
{ sleep 18;echo "screendump $out/visual-setup.ppm";keys axiomadmin;sleep 1;keys axiomtest123;sleep 1;keys axiomtest123;sleep 1;echo "screendump $out/visual-recovery.ppm";keys '';sleep 1;echo "screendump $out/visual-login.ppm";keys axiomadmin;sleep 1;keys axiomtest123;sleep 3;echo "screendump $out/visual-terminal.ppm";sleep 1;echo quit;} | timeout 48 "$qemu" -M q35 -smp 4 -m 512M -cdrom "$iso" -boot d -display none -serial "file:$out/visual-login.log" -monitor stdio -no-reboot >/dev/null 2>&1||true
grep -q 'first administrator account created' "$out/visual-login.log"
grep -q 'one-time recovery key acknowledged' "$out/visual-login.log"
grep -q 'graphical login successful' "$out/visual-login.log"
grep -q 'launching user-space init and shell' "$out/visual-login.log"
! grep -q '\[ERROR\]' "$out/visual-login.log"
echo "PASS: graphical setup, login, and terminal transition"
