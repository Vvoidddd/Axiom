AXIOM_ROOT ?= ..
CC ?= gcc
LD ?= ld
CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -I$(AXIOM_ROOT)/sdk/include -I$(AXIOM_ROOT)/user
LDFLAGS := -m elf_x86_64 -nostdlib -static -z noexecstack -T $(AXIOM_ROOT)/user/linker.ld
OBJECTS := $(SOURCES:.c=.o)
$(APP): $(OBJECTS) $(AXIOM_ROOT)/build/user/crt0.o $(AXIOM_ROOT)/build/user/libaxiom.o
	$(LD) $(LDFLAGS) $(AXIOM_ROOT)/build/user/crt0.o $(AXIOM_ROOT)/build/user/libaxiom.o $(OBJECTS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
