.SUFFIXES:

LIMINE_VERSION := v12.7.0
LIMINE_PROTOCOL_COMMIT := 93b5284ee279c817f3ad7a640472a38485c113c3
BUILD := build
KERNEL := $(BUILD)/axiom.elf
ISO := $(BUILD)/axiom.iso
LIMINE := $(BUILD)/limine-binary
ISO_ROOT := $(BUILD)/iso_root

CC ?= gcc
LD ?= ld
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Werror -ffreestanding \
	-fno-stack-protector -fno-stack-check -fno-lto -fno-pic \
	-ffunction-sections -fdata-sections -m64 -march=x86-64 -mabi=sysv \
	-mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel \
	-mgeneral-regs-only \
	-Isrc -Isdk/include -I$(LIMINE)
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 \
	-z noexecstack --gc-sections -T linker.ld

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(SOURCES))
ASM_SOURCES := $(wildcard src/*.S)
OBJECTS += $(patsubst src/%.S,$(BUILD)/obj/%.S.o,$(ASM_SOURCES))
USER_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -Isdk/include -Iuser
USER_LDFLAGS := -m elf_x86_64 -nostdlib -static -z noexecstack -T user/linker.ld
USER_COMMON := $(BUILD)/user/crt0.o $(BUILD)/user/libaxiom.o
USER_BINS := $(BUILD)/user/init.elf $(BUILD)/user/shell.elf $(BUILD)/user/hello.elf

.PHONY: all iso run-bios run-uefi test clean distclean
all: $(KERNEL)
iso: $(ISO)

$(BUILD)/obj/%.o: src/%.c $(LIMINE)/limine.h
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/obj/%.S.o: src/%.S
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user/%.o: user/%.c
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/user/%.o: user/%.S
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/user/init.elf: $(USER_COMMON) $(BUILD)/user/init.o user/linker.ld
	$(LD) $(USER_LDFLAGS) $(USER_COMMON) $(BUILD)/user/init.o -o $@

$(BUILD)/user/shell.elf: $(USER_COMMON) $(BUILD)/user/shell.o user/linker.ld
	$(LD) $(USER_LDFLAGS) $(USER_COMMON) $(BUILD)/user/shell.o -o $@

$(BUILD)/user/hello.elf: $(USER_COMMON) $(BUILD)/user/hello.o user/linker.ld
	$(LD) $(USER_LDFLAGS) $(USER_COMMON) $(BUILD)/user/hello.o -o $@

$(BUILD)/obj/user_bins.S.o: $(USER_BINS)

-include $(OBJECTS:.o=.d)

$(KERNEL): $(OBJECTS) linker.ld
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(LIMINE)/limine.h:
	mkdir -p $(LIMINE)
	curl -fL https://raw.githubusercontent.com/Limine-Bootloader/limine-protocol/$(LIMINE_PROTOCOL_COMMIT)/include/limine.h -o $@

$(LIMINE)/limine: $(LIMINE)/limine.h
	curl -fL https://github.com/Limine-Bootloader/Limine/releases/download/$(LIMINE_VERSION)/limine-binary.tar.gz -o $(BUILD)/limine.tar.gz
	tar -xzf $(BUILD)/limine.tar.gz -C $(BUILD)
	$(MAKE) -C $(LIMINE)

$(ISO): $(KERNEL) $(LIMINE)/limine limine.conf
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/axiom.elf
	cp limine.conf $(LIMINE)/limine-bios.sys $(LIMINE)/limine-bios-cd.bin $(LIMINE)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $@
	$(LIMINE)/limine bios-install $@

run-bios: $(ISO)
	qemu-system-x86_64 -M q35 -m 256M -cdrom $(ISO) -boot d

run-uefi: $(ISO)
	@test -f /usr/share/edk2/x64/OVMF_CODE.fd || (echo "Install edk2-ovmf first"; exit 1)
	qemu-system-x86_64 -M q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.fd \
		-cdrom $(ISO) -boot d

test: $(ISO)
	./tests/smoke.sh $(ISO)

clean:
	rm -rf $(BUILD)/obj $(BUILD)/user $(KERNEL) $(ISO) $(ISO_ROOT)

distclean:
	rm -rf $(BUILD)
