# Makefile
# Author: s0g3king

all:
	gcc -Wall -Wextra -g -o riscv-emulator main.c

clean:
	rm -f riscv-emulator


# assemble demo.s into a raw binary and run it in the emulator
demo: all
	clang --target=riscv32 -march=rv32i -c demo.s -o demo.o
	llvm-objcopy -O binary demo.o demo.bin
	rm -f demo.o
