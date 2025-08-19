/*
 * main.c
 * Author: s0g3king
 * Simple RISC-V (RV32I) emulator: memory, registers, fetch/decode/execute
 */

#include "instruction_types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE (1024 * 1024) // 1 MB

uint32_t pc = 0;
uint32_t regs[32];
uint8_t memory[MEM_SIZE]; // byte-addressable

int halted = 0;         // set by ECALL exit and EBREAK
uint32_t exit_code = 0; // a0 when the program calls exit

int print_instructions = 0; // --print-instructions
int print_registers = 0;    // --print-registers

#define REG_ZERO 0

// only prints when --print-instructions is on
#define LOG(...)                                                                                   \
    do {                                                                                           \
        if (print_instructions)                                                                    \
            printf(__VA_ARGS__);                                                                   \
    } while (0)

// ABI names of the registers
const char *reg_names[32] = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
                             "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                             "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

uint32_t load_word(uint32_t addr) {
    if (addr + 3 >= MEM_SIZE) { // out of bounds
        printf("[-]\t Load out of bounds: 0x%x\n", addr);
        exit(1);
    }
    return memory[addr] | (memory[addr + 1] << 8) | (memory[addr + 2] << 16) |
           ((uint32_t)memory[addr + 3] << 24);
}

void store_word(uint32_t addr, uint32_t val) {
    if (addr + 3 >= MEM_SIZE) { // out of bounds
        printf("[-]\t Store out of bounds: 0x%x\n", addr);
        exit(1);
    }
    memory[addr] = val & 0xff;
    memory[addr + 1] = (val >> 8) & 0xff;
    memory[addr + 2] = (val >> 16) & 0xff;
    memory[addr + 3] = (val >> 24) & 0xff;
}

uint32_t load_byte(uint32_t addr) {
    if (addr >= MEM_SIZE) { // out of bounds
        printf("[-]\t Load out of bounds: 0x%x\n", addr);
        exit(1);
    }
    return memory[addr];
}

uint32_t load_half(uint32_t addr) {
    if (addr + 1 >= MEM_SIZE) { // out of bounds
        printf("[-]\t Load out of bounds: 0x%x\n", addr);
        exit(1);
    }
    return memory[addr] | (memory[addr + 1] << 8);
}

void store_byte(uint32_t addr, uint32_t val) {
    if (addr >= MEM_SIZE) { // out of bounds
        printf("[-]\t Store out of bounds: 0x%x\n", addr);
        exit(1);
    }
    memory[addr] = val & 0xff;
}

void store_half(uint32_t addr, uint32_t val) {
    if (addr + 1 >= MEM_SIZE) { // out of bounds
        printf("[-]\t Store out of bounds: 0x%x\n", addr);
        exit(1);
    }
    memory[addr] = val & 0xff;
    memory[addr + 1] = (val >> 8) & 0xff;
}

uint32_t fetch() { return load_word(pc); }

int32_t sign_extend(uint32_t val, int bits) {
    int32_t m = 1u << (bits - 1);
    return (val ^ m) - m;
}

void decode_instruction(uint32_t inst, void *decoded_inst) {
    uint32_t opcode = inst & 0x7f; // The last 7 bits contain the opcode

    // Handle R-type (opcode: 0x33)
    if (opcode == 0x33) {
        RType *r = (RType *)decoded_inst;
        r->opcode = opcode;
        r->rd = (inst >> 7) & 0x1f;
        r->funct3 = (inst >> 12) & 0x7;
        r->rs1 = (inst >> 15) & 0x1f;
        r->rs2 = (inst >> 20) & 0x1f;
        r->funct7 = (inst >> 25) & 0x7f;
    }
    // Handle I-type (opcode: 0x13, 0x03, 0x67, 0x73, 0x0f)
    else if (opcode == 0x13 || opcode == 0x03 || opcode == 0x67 || opcode == 0x73 ||
             opcode == 0x0f) {
        IType *i = (IType *)decoded_inst;
        i->opcode = opcode;
        i->rd = (inst >> 7) & 0x1f;
        i->funct3 = (inst >> 12) & 0x7;
        i->rs1 = (inst >> 15) & 0x1f;
        i->imm = (inst >> 20);
    }
    // Handle S-type (opcode: 0x23)
    else if (opcode == 0x23) {
        SType *s = (SType *)decoded_inst;
        s->opcode = opcode;
        s->funct3 = (inst >> 12) & 0x7;
        s->rs1 = (inst >> 15) & 0x1f;
        s->rs2 = (inst >> 20) & 0x1f;
        s->imm = ((inst >> 7) & 0x1f) | (((inst >> 25) & 0x7f) << 5);
    }
    // Handle B-type (opcode: 0x63)
    else if (opcode == 0x63) {
        BType *b = (BType *)decoded_inst;
        b->opcode = opcode;
        b->funct3 = (inst >> 12) & 0x7;
        b->rs1 = (inst >> 15) & 0x1f;
        b->rs2 = (inst >> 20) & 0x1f;
        b->imm = ((inst >> 8) & 0xf) | (((inst >> 25) & 0x3f) << 4) | (((inst >> 7) & 0x1) << 10) |
                 ((inst >> 31) << 11);
    }
    // Handle U-type (opcode: 0x37, 0x17)
    else if (opcode == 0x37 || opcode == 0x17) {
        UType *u = (UType *)decoded_inst;
        u->opcode = opcode;
        u->rd = (inst >> 7) & 0x1f;
        u->imm = (inst >> 12) & 0xfffff; // Upper 20 bits
    }
    // Handle J-type (opcode: 0x6f)
    else if (opcode == 0x6f) {
        JType *j = (JType *)decoded_inst;
        j->opcode = opcode;
        j->rd = (inst >> 7) & 0x1f;
        j->imm = ((inst >> 21) & 0x3ff) | ((inst >> 20) & 0x1) << 10 | ((inst >> 12) & 0xff) << 11 |
                 ((inst >> 31) << 19);
    }
}

void execute(void *decoded_inst) {
    uint32_t opcode = *((uint32_t *)decoded_inst) & 0x7f; // get opcode
    uint32_t next_pc = pc + 4;                            // branches and jumps change this

    switch (opcode) {
    case 0x33: // R-type
    {
        RType *r = (RType *)decoded_inst;
        // Handle R-type instruction (e.g. ADD, SUB, etc)
        if (r->funct3 == 0x0) { // ADD/SUB
            if (r->funct7 == 0x0) {
                regs[r->rd] = regs[r->rs1] + regs[r->rs2];
                LOG("[*]\t ADD x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
            } else if (r->funct7 == 0x20) {
                regs[r->rd] = regs[r->rs1] - regs[r->rs2];
                LOG("[*]\t SUB x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
            }
        } else if (r->funct3 == 0x1) { // SLL (shift amount is the low 5 bits)
            regs[r->rd] = regs[r->rs1] << (regs[r->rs2] & 0x1f);
            LOG("[*]\t SLL x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else if (r->funct3 == 0x2) { // SLT (signed compare)
            regs[r->rd] = (int32_t)regs[r->rs1] < (int32_t)regs[r->rs2] ? 1 : 0;
            LOG("[*]\t SLT x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else if (r->funct3 == 0x3) { // SLTU (unsigned compare)
            regs[r->rd] = regs[r->rs1] < regs[r->rs2] ? 1 : 0;
            LOG("[*]\t SLTU x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else if (r->funct3 == 0x4) { // XOR
            regs[r->rd] = regs[r->rs1] ^ regs[r->rs2];
            LOG("[*]\t XOR x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else if (r->funct3 == 0x5) { // SRL/SRA
            if (r->funct7 == 0x0) {
                regs[r->rd] = regs[r->rs1] >> (regs[r->rs2] & 0x1f);
                LOG("[*]\t SRL x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
            } else if (r->funct7 == 0x20) {
                regs[r->rd] = (int32_t)regs[r->rs1] >> (regs[r->rs2] & 0x1f);
                LOG("[*]\t SRA x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
            }
        } else if (r->funct3 == 0x6) { // OR
            regs[r->rd] = regs[r->rs1] | regs[r->rs2];
            LOG("[*]\t OR x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else if (r->funct3 == 0x7) { // AND
            regs[r->rd] = regs[r->rs1] & regs[r->rs2];
            LOG("[*]\t AND x%d, x%d, x%d\n", r->rd, r->rs1, r->rs2);
        } else {
            puts("[-]\t Unknown Instruction");
        }
        break;
    }
    case 0x13: // I-type
    {
        IType *i = (IType *)decoded_inst;
        // Handle I-type instructions (e.g. ADDI)
        if (i->funct3 == 0x0) { // ADDI
            regs[i->rd] = (int32_t)regs[i->rs1] + sign_extend(i->imm, 12);
            LOG("[*]\t ADDI x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        } else if (i->funct3 == 0x1) { // SLLI (shift amount is the low 5 bits of the imm)
            regs[i->rd] = regs[i->rs1] << (i->imm & 0x1f);
            LOG("[*]\t SLLI x%d, x%d, %d\n", i->rd, i->rs1, i->imm & 0x1f);
        } else if (i->funct3 == 0x2) { // SLTI (signed compare)
            regs[i->rd] = (int32_t)regs[i->rs1] < sign_extend(i->imm, 12) ? 1 : 0;
            LOG("[*]\t SLTI x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        } else if (i->funct3 == 0x3) { // SLTIU (imm is sign extended, then compared as unsigned)
            regs[i->rd] = regs[i->rs1] < (uint32_t)sign_extend(i->imm, 12) ? 1 : 0;
            LOG("[*]\t SLTIU x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        } else if (i->funct3 == 0x4) { // XORI
            regs[i->rd] = regs[i->rs1] ^ sign_extend(i->imm, 12);
            LOG("[*]\t XORI x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        } else if (i->funct3 == 0x5) { // SRLI/SRAI (bit 10 of the imm picks arithmetic)
            if ((i->imm & 0x400) == 0) {
                regs[i->rd] = regs[i->rs1] >> (i->imm & 0x1f);
                LOG("[*]\t SRLI x%d, x%d, %d\n", i->rd, i->rs1, i->imm & 0x1f);
            } else {
                regs[i->rd] = (int32_t)regs[i->rs1] >> (i->imm & 0x1f);
                LOG("[*]\t SRAI x%d, x%d, %d\n", i->rd, i->rs1, i->imm & 0x1f);
            }
        } else if (i->funct3 == 0x6) { // ORI
            regs[i->rd] = regs[i->rs1] | sign_extend(i->imm, 12);
            LOG("[*]\t ORI x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        } else if (i->funct3 == 0x7) { // ANDI
            regs[i->rd] = regs[i->rs1] & sign_extend(i->imm, 12);
            LOG("[*]\t ANDI x%d, x%d, %d\n", i->rd, i->rs1, sign_extend(i->imm, 12));
        }
        break;
    }
    case 0x03: // I-type loads
    {
        IType *i = (IType *)decoded_inst;
        uint32_t addr = regs[i->rs1] + sign_extend(i->imm, 12);
        if (i->funct3 == 0x0) { // LB
            regs[i->rd] = sign_extend(load_byte(addr), 8);
            LOG("[*]\t LB x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        } else if (i->funct3 == 0x1) { // LH
            regs[i->rd] = sign_extend(load_half(addr), 16);
            LOG("[*]\t LH x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        } else if (i->funct3 == 0x2) { // LW
            regs[i->rd] = load_word(addr);
            LOG("[*]\t LW x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        } else if (i->funct3 == 0x4) { // LBU (zero extended)
            regs[i->rd] = load_byte(addr);
            LOG("[*]\t LBU x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        } else if (i->funct3 == 0x5) { // LHU (zero extended)
            regs[i->rd] = load_half(addr);
            LOG("[*]\t LHU x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        } else {
            puts("[-]\t Unknown Instruction");
        }
        break;
    }
    case 0x23: // S-type
    {
        SType *s = (SType *)decoded_inst;
        // Handle S-type instructions (SB, SH, SW)
        uint32_t addr = regs[s->rs1] + sign_extend(s->imm, 12);
        if (s->funct3 == 0x0) { // SB
            store_byte(addr, regs[s->rs2]);
            LOG("[*]\t SB x%d, %d(x%d)\n", s->rs2, sign_extend(s->imm, 12), s->rs1);
        } else if (s->funct3 == 0x1) { // SH
            store_half(addr, regs[s->rs2]);
            LOG("[*]\t SH x%d, %d(x%d)\n", s->rs2, sign_extend(s->imm, 12), s->rs1);
        } else if (s->funct3 == 0x2) { // SW
            store_word(addr, regs[s->rs2]);
            LOG("[*]\t SW x%d, %d(x%d)\n", s->rs2, sign_extend(s->imm, 12), s->rs1);
        } else {
            puts("[-]\t Unknown Instruction");
        }
        break;
    }
    case 0x63: // B-type
    {
        BType *b = (BType *)decoded_inst;
        // the decoded imm is the offset without its lowest bit, so shift it back
        int32_t offset = sign_extend(b->imm << 1, 13);
        int taken = 0;
        if (b->funct3 == 0x0) { // BEQ
            taken = regs[b->rs1] == regs[b->rs2];
            LOG("[*]\t BEQ x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else if (b->funct3 == 0x1) { // BNE
            taken = regs[b->rs1] != regs[b->rs2];
            LOG("[*]\t BNE x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else if (b->funct3 == 0x4) { // BLT (signed)
            taken = (int32_t)regs[b->rs1] < (int32_t)regs[b->rs2];
            LOG("[*]\t BLT x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else if (b->funct3 == 0x5) { // BGE (signed)
            taken = (int32_t)regs[b->rs1] >= (int32_t)regs[b->rs2];
            LOG("[*]\t BGE x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else if (b->funct3 == 0x6) { // BLTU
            taken = regs[b->rs1] < regs[b->rs2];
            LOG("[*]\t BLTU x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else if (b->funct3 == 0x7) { // BGEU
            taken = regs[b->rs1] >= regs[b->rs2];
            LOG("[*]\t BGEU x%d, x%d, %d\n", b->rs1, b->rs2, offset);
        } else {
            puts("[-]\t Unknown Instruction");
        }
        if (taken)
            next_pc = pc + offset;
        break;
    }
    case 0x6f: // J-type (JAL)
    {
        JType *j = (JType *)decoded_inst;
        // same as the branches, the decoded imm is shifted by one
        int32_t offset = sign_extend(j->imm << 1, 21);
        regs[j->rd] = pc + 4; // return address
        next_pc = pc + offset;
        LOG("[*]\t JAL x%d, %d\n", j->rd, offset);
        break;
    }
    case 0x67: // I-type (JALR)
    {
        IType *i = (IType *)decoded_inst;
        uint32_t target =
            (regs[i->rs1] + sign_extend(i->imm, 12)) & ~1u; // read rs1 before rd is written
        regs[i->rd] = pc + 4;                               // return address
        next_pc = target;
        LOG("[*]\t JALR x%d, %d(x%d)\n", i->rd, sign_extend(i->imm, 12), i->rs1);
        break;
    }
    case 0x17: // U-type  (Since there's only two UType instructions I'll just separate them by
               // opcode)
    {
        UType *u = (UType *)decoded_inst;
        // Handle U-type instruction AUIPC
        regs[u->rd] = pc + sign_extend(u->imm << 12, 32);
        LOG("[*]\t AUIPC x%d, %d\n", u->rd, sign_extend(u->imm << 12, 32));
        break;
    }
    case 0x37: // U-type
    {
        UType *u = (UType *)decoded_inst;
        // Handle U-tyoe instruction LUI
        regs[u->rd] = sign_extend(u->imm << 12, 32);
        LOG("[*]\t LUI x%d, %d\n", u->rd, sign_extend(u->imm << 12, 32));
        break;
    }
    case 0x0f: // FENCE (only one hart and no caches, so it does nothing)
        LOG("[*]\t FENCE\n");
        break;
    case 0x73: // ECALL/EBREAK
    {
        IType *i = (IType *)decoded_inst;
        if (i->imm == 0x0) {      // ECALL
            if (regs[17] == 93) { // a7 = 93 is the exit syscall, a0 is the exit code
                exit_code = regs[10];
                halted = 1;
                LOG("[*]\t ECALL exit(%d)\n", (int32_t)exit_code);
            } else {
                printf("[-]\t ECALL unknown syscall %d\n", (int32_t)regs[17]);
            }
        } else if (i->imm == 0x1) { // EBREAK
            LOG("[*]\t EBREAK\n");
            halted = 1;
        } else {
            puts("[-]\t Unknown Instruction");
        }
        break;
    }
    default:
        printf("[-]\t Unknown opcode: %x\n", opcode);
        break;
    }

    pc = next_pc; // move to the next instruction
    regs[REG_ZERO] = 0;
}

void *allocate_instruction(uint32_t opcode) {
    switch (opcode) {
    case 0x33: // R-type
        return malloc(sizeof(RType));
    case 0x13: // I-type (e.g. ADDI)
    case 0x03: // I-type (e.g. LOADs)
    case 0x67: // I-type (JALR)
    case 0x73: // I-type (ECALL/EBREAK)
    case 0x0f: // I-type (FENCE)
        return malloc(sizeof(IType));
    case 0x23: // S-type
        return malloc(sizeof(SType));
    case 0x63: // B-type
        return malloc(sizeof(BType));
    case 0x37: // U-type (LUI)
    case 0x17: // U-type (AUIPC)
        return malloc(sizeof(UType));
    case 0x6f: // J-type (JAL)
        return malloc(sizeof(JType));
    default:
        return NULL;
    }
}

void display_registers() {
    puts("\n[*] Registers:");
    printf("    pc = 0x%08x\n\n", pc);
    // two columns, x0-x15 on the left and x16-x31 on the right
    for (int i = 0; i < 16; i++) {
        printf("    x%-2d %-4s 0x%08x %11d", i, reg_names[i], regs[i], (int32_t)regs[i]);
        printf("    x%-2d %-4s 0x%08x %11d\n", i + 16, reg_names[i + 16], regs[i + 16],
               (int32_t)regs[i + 16]);
    }
}

int main(int argc, char *argv[]) {

    const char *program = NULL;

    // handle the arguments, anything that is not a flag is the program
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--print-instructions") == 0) {
            print_instructions = 1;
        } else if (strcmp(argv[a], "--print-registers") == 0) {
            print_registers = 1;
        } else if (strncmp(argv[a], "--", 2) == 0) {
            printf("[-] Unknown option %s\n", argv[a]);
            return 1;
        } else {
            program = argv[a];
        }
    }

    if (program == NULL) {
        puts("[-] needs input program");
        printf("    Usage: %s [--print-instructions] [--print-registers] <program>\n", argv[0]);
        return 1;
    }

    puts("[*] RISC-V Enulator\n");

    puts("[*] Loading Program");

    // Load the instructions into memory
    FILE *fp = fopen(program, "r");
    if (fp == NULL) {
        printf("[-] Error opening file %s\n", program);
        return 1;
    }

    int ch;

    // reuse PC for loading in the program
    while ((ch = fgetc(fp)) != EOF && pc < MEM_SIZE) {
        memory[pc] = ch;
        pc++;
    }

    fclose(fp);
    // reset PC to 0
    pc = 0;

    if (print_instructions)
        puts("[*] Instructions Executed:\n");

    while (pc < MEM_SIZE && !halted) {
        uint32_t inst = load_word(pc);
        if (inst == 0)
            break; // halt condition

        uint32_t opcode = inst & 0x7f;

        void *decoded_inst = allocate_instruction(opcode);
        if (decoded_inst == NULL) { // unknown opcode, nothing to decode
            printf("[-]\t Unknown opcode: %x\n", opcode);
            break;
        }

        decode_instruction(inst, decoded_inst);
        execute(decoded_inst);

        free(decoded_inst);
    }

    if (print_registers)
        display_registers();

    printf("\n[*] Exit code: %d\n", (int32_t)exit_code);

    return exit_code;
}
