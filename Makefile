# Makefile
# Author: s0g3king

all:
	gcc -Wall -Wextra -g -o riscv-emulator main.c

clean:
	rm -f riscv-emulator

