#!/usr/bin/env bash
set -euo pipefail

iso=${1:-axiom.iso}
qemu=${QEMU:-qemu-system-x86_64}
ovmf=${OVMF:-}
out=${2:-build/security}

keys(){ local text=$1 char;for((i=0;i<${#text};i++));do char=${text:i:1};case $char in ' ')char=spc;; '-')char=minus;; '/')char=slash;; esac;printf 'sendkey %s\n' "$char";sleep .035;done;echo sendkey ret;sleep .15;}

run_case(){
 local firmware=$1 wait_time=18 log="$out-$1.log";local -a fw=()
 [[ $firmware == uefi ]]&&{ wait_time=21;fw=(-drive "if=pflash,format=raw,readonly=on,file=$ovmf");}
 rm -f "$log" "$out-$firmware-standard.ppm" "$out-$firmware-disabled.ppm" "$out-$firmware-revoked.ppm"
 {
  sleep "$wait_time"
  keys axiomadmin;sleep .7;keys axiomtest123;sleep .7;keys axiomtest123;sleep 1;keys '';sleep 1
  keys axiomadmin;sleep .7;keys axiomtest123;sleep 2
  keys 'useradd intruder intruder12';sleep .5
  keys 'elevate users wrongpass';sleep 1
  keys 'elevate users axiomtest123';sleep .5
  keys securitytest;sleep .5
  keys 'elevate users axiomtest123';sleep .5
  keys 'useradd guest guestpass12 standard';sleep .5
  keys 'touch /users/axiomadmin/private';keys 'chmod 600 /users/axiomadmin/private';sleep .5
  keys 'elevate power axiomtest123';keys reboot;keys 'echo still-running';sleep .7
  keys logout;sleep 1.5
  keys guest;sleep .7;keys guestpass12;sleep 2
  keys 'useradd intruder intruder12';keys 'elevate users guestpass12';keys 'cat /users/axiomadmin/private';sleep .7
  echo "screendump $out-$firmware-standard.ppm"
  keys logout;sleep 1.5
  keys axiomadmin;sleep .7;keys axiomtest123;sleep 2
  keys 'elevate users axiomtest123';sleep .5;keys 'usermod guest disable --confirm';sleep .5;keys logout;sleep 1.5
  keys guest;sleep .7;keys guestpass12;sleep 1.5
  echo "screendump $out-$firmware-disabled.ppm"
  keys axiomadmin;sleep .7;keys axiomtest123;sleep 2
  keys 'elevate users axiomtest123';sleep .5;keys 'useradd safeguard safepass12 admin';sleep .5
  keys 'elevate power axiomtest123';sleep .5;keys 'usermod axiomadmin standard --confirm';sleep .5
  keys 'reboot --confirm';keys 'echo revocation-enforced';sleep .7
  echo "screendump $out-$firmware-revoked.ppm";echo quit
 } | timeout 105 "$qemu" -M q35 -smp 4 -m 512M "${fw[@]}" -cdrom "$iso" -boot d -display none -serial "file:$log" -monitor stdio -no-reboot >/dev/null 2>&1||true
 grep -q '\[AUDIT\] elevation-denied' "$log"
 grep -q '\[AUDIT\] elevation-granted' "$log"
 grep -q '\[AUDIT\] permission-denied' "$log"
 grep -q '\[AUDIT\] account-create' "$log"
 grep -q '\[AUDIT\] account-disabled' "$log"
 grep -q '\[AUDIT\] login-blocked' "$log"
 grep -q '\[PASS\] SECURITY CAPABILITY EXPIRY TEST' "$log"
 grep -q 'random recovery credential and database fallback verified' "$log"
 grep -q 'concurrent login serialization verified on all processors' "$log"
 grep -q '\[CMD\] echo still-running' "$log"
 grep -q '\[CMD\] echo revocation-enforced' "$log"
 ! grep -q '\[ERROR\] CPU exception' "$log"
 test -s "$out-$firmware-standard.ppm";test -s "$out-$firmware-disabled.ppm";test -s "$out-$firmware-revoked.ppm"
 echo "PASS: $firmware privilege and account isolation"
}

command -v "$qemu" >/dev/null
[[ -n $ovmf && -f $ovmf ]]||{ echo 'Set OVMF to an OVMF_CODE firmware file' >&2;exit 1;}
mkdir -p "$(dirname "$out")"
mode=${3:-all}
[[ $mode == all || $mode == bios ]]&&run_case bios
[[ $mode == all || $mode == uefi ]]&&run_case uefi
echo 'All Axiom security tests passed.'
