/*
 * instruction_types.h
 * Author: s0g3king
 * Structs for the RISC-V instruction formats (R, I, S, B, U, J)
 */

#ifndef INSTRUCTION_TYPES_H
#define INSTRUCTION_TYPES_H

#include <stdint.h>
#include <stdio.h>

// R-type: register-register arithmetic
typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t funct3;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t funct7;
} RType;

// I-type: immediate arithmetic, loads
typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t funct3;
    uint32_t rs1;
    uint32_t imm;
} IType;

// S-type: stores
typedef struct {
    uint32_t opcode;
    uint32_t funct3;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t imm;
} SType;

// B-type: conditional branches
typedef struct {
    uint32_t opcode;
    uint32_t funct3;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t imm;
} BType;

// U-type: upper immediate instructions
typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t imm;
} UType;

// J-type: jump (JAL)
typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t imm;
} JType;


#endif // INSTRUCTION_TYPES_H
